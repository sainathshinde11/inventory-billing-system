#include "Billing.h"
#include <filesystem>
#include <fstream>
#include <iomanip>

int BillingService::createBill(const std::string& customer, const std::vector<CartItem>& cart,
                               double discountPct) {
    if (cart.empty()) throw std::invalid_argument("Cart is empty");
    if (discountPct < 0 || discountPct > 100) throw std::invalid_argument("Discount must be 0-100");

    db_.begin();
    try {
        Statement insBill(db_, "INSERT INTO bills (customer) VALUES (?)");
        insBill.bindText(1, customer.empty() ? "Walk-in" : customer).run();
        int billId = db_.lastInsertId();

        double subtotal = 0, gstSum = 0;
        for (const auto& item : cart) {
            if (item.quantity <= 0) throw std::invalid_argument("Quantity must be positive");
            auto p = inv_.find(item.productId);
            if (!p) throw std::runtime_error("Product " + std::to_string(item.productId) + " not found");

            // Conditional UPDATE: succeeds only if enough stock exists -> no overselling.
            Statement dec(db_, "UPDATE products SET quantity = quantity - ? "
                               "WHERE id = ? AND quantity >= ?");
            dec.bindInt(1, item.quantity).bindInt(2, item.productId).bindInt(3, item.quantity).run();
            if (db_.changes() != 1)
                throw std::runtime_error("Insufficient stock for '" + p->name + "' (available: " +
                                         std::to_string(p->quantity) + ")");

            double line = item.quantity * p->price;
            subtotal += line;
            gstSum   += line * p->gstPercent / 100.0;

            Statement insItem(db_, "INSERT INTO bill_items "
                                   "(bill_id, product_id, name, quantity, unit_price, gst_percent, line_total) "
                                   "VALUES (?, ?, ?, ?, ?, ?, ?)");
            insItem.bindInt(1, billId).bindInt(2, p->id).bindText(3, p->name).bindInt(4, item.quantity)
                   .bindDouble(5, p->price).bindDouble(6, p->gstPercent).bindDouble(7, line).run();
        }

        double discount = subtotal * discountPct / 100.0;
        double gst      = gstSum * (1.0 - discountPct / 100.0);   // GST on discounted value
        double total    = subtotal - discount + gst;

        Statement upd(db_, "UPDATE bills SET subtotal=?, discount=?, gst=?, total=? WHERE id=?");
        upd.bindDouble(1, subtotal).bindDouble(2, discount).bindDouble(3, gst)
           .bindDouble(4, total).bindInt(5, billId).run();

        db_.commit();
        return billId;
    } catch (...) {
        db_.rollback();
        throw;
    }
}

Bill BillingService::getBill(int id) {
    Bill b;
    {
        Statement st(db_, "SELECT id, customer, created_at, subtotal, discount, gst, total "
                          "FROM bills WHERE id = ?");
        st.bindInt(1, id);
        if (!st.step()) throw std::runtime_error("Bill " + std::to_string(id) + " not found");
        b.id = st.getInt(0); b.customer = st.getText(1); b.createdAt = st.getText(2);
        b.subtotal = st.getDouble(3); b.discount = st.getDouble(4);
        b.gst = st.getDouble(5); b.total = st.getDouble(6);
    }
    Statement li(db_, "SELECT product_id, name, quantity, unit_price, gst_percent, line_total "
                      "FROM bill_items WHERE bill_id = ? ORDER BY id");
    li.bindInt(1, id);
    while (li.step()) {
        BillLine l;
        l.productId = li.getInt(0); l.name = li.getText(1); l.quantity = li.getInt(2);
        l.unitPrice = li.getDouble(3); l.gstPercent = li.getDouble(4); l.lineTotal = li.getDouble(5);
        b.lines.push_back(l);
    }
    return b;
}

std::vector<Bill> BillingService::recentBills(int limit) {
    std::vector<Bill> out;
    Statement st(db_, "SELECT id, customer, created_at, subtotal, discount, gst, total "
                      "FROM bills ORDER BY id DESC LIMIT ?");
    st.bindInt(1, limit);
    while (st.step()) {
        Bill b;
        b.id = st.getInt(0); b.customer = st.getText(1); b.createdAt = st.getText(2);
        b.subtotal = st.getDouble(3); b.discount = st.getDouble(4);
        b.gst = st.getDouble(5); b.total = st.getDouble(6);
        out.push_back(b);
    }
    return out;
}

SalesReport BillingService::salesReport() {
    SalesReport r;
    {
        Statement st(db_, "SELECT COUNT(*), COALESCE(SUM(total),0), "
                          "COALESCE(SUM(CASE WHEN date(created_at)=date('now','localtime') "
                          "THEN total END),0) FROM bills");
        st.step();
        r.billCount = st.getInt(0); r.totalRevenue = st.getDouble(1); r.todayRevenue = st.getDouble(2);
    }
    Statement top(db_, "SELECT name, SUM(quantity) AS sold FROM bill_items "
                       "GROUP BY product_id, name ORDER BY sold DESC LIMIT 5");
    while (top.step()) r.topProducts.emplace_back(top.getText(0), top.getInt(1));
    return r;
}

std::string BillingService::writeReceipt(int billId, const std::string& dir) {
    Bill b = getBill(billId);
    std::filesystem::create_directories(dir);
    std::string path = dir + "/bill_" + std::to_string(b.id) + ".txt";
    std::ofstream out(path);
    if (!out) throw std::runtime_error("Cannot write receipt: " + path);

    const std::string line(64, '-');
    out << std::fixed << std::setprecision(2);
    out << line << "\n                    INVENTORY & BILLING SYSTEM\n" << line << "\n";
    out << "Bill No : " << b.id << "\nDate    : " << b.createdAt
        << "\nCustomer: " << b.customer << "\n" << line << "\n";
    out << std::left << std::setw(24) << "Item" << std::right << std::setw(5) << "Qty"
        << std::setw(10) << "Rate" << std::setw(7) << "GST%" << std::setw(12) << "Amount" << "\n" << line << "\n";
    for (const auto& l : b.lines) {
        out << std::left << std::setw(24) << l.name.substr(0, 23) << std::right << std::setw(5) << l.quantity
            << std::setw(10) << l.unitPrice << std::setw(7) << l.gstPercent << std::setw(12) << l.lineTotal << "\n";
    }
    out << line << "\n";
    out << std::setw(52) << "Subtotal :" << std::setw(12) << b.subtotal << "\n";
    out << std::setw(52) << "Discount :" << std::setw(12) << -b.discount << "\n";
    out << std::setw(52) << "GST      :" << std::setw(12) << b.gst << "\n";
    out << std::setw(52) << "TOTAL    :" << std::setw(12) << b.total << "\n" << line << "\n";
    out << "              Thank you! Visit again.\n";
    return path;
}