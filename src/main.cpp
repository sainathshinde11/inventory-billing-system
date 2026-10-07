// Inventory & Billing Management System - console UI
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include "Billing.h"
#include "Database.h"
#include "Inventory.h"

// ---------- input helpers (validated, EOF-safe) ----------
struct InputClosed {};

static std::string readLine(const std::string& prompt) {
    std::cout << prompt << std::flush;
    std::string s;
    if (!std::getline(std::cin, s)) throw InputClosed{};
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

static int readInt(const std::string& prompt, int lo = std::numeric_limits<int>::min(),
                   int hi = std::numeric_limits<int>::max()) {
    while (true) {
        std::string s = readLine(prompt);
        try {
            size_t pos = 0;
            int v = std::stoi(s, &pos);
            if (pos == s.size() && v >= lo && v <= hi) return v;
        } catch (...) {}
        std::cout << "  Enter a whole number";
        if (lo != std::numeric_limits<int>::min()) std::cout << " between " << lo << " and " << hi;
        std::cout << ".\n";
    }
}

static double readDouble(const std::string& prompt, double lo = 0, double hi = 1e9) {
    while (true) {
        std::string s = readLine(prompt);
        try {
            size_t pos = 0;
            double v = std::stod(s, &pos);
            if (pos == s.size() && v >= lo && v <= hi) return v;
        } catch (...) {}
        std::cout << "  Enter a number between " << lo << " and " << hi << ".\n";
    }
}

// ---------- display helpers ----------
static void printProducts(const std::vector<Product>& ps) {
    if (ps.empty()) { std::cout << "No products found.\n"; return; }
    std::cout << std::left << std::setw(5) << "ID" << std::setw(24) << "Name" << std::right
              << std::setw(10) << "Price" << std::setw(8) << "Stock" << std::setw(9) << "Reorder"
              << std::setw(7) << "GST%" << "  Status\n" << std::string(72, '-') << "\n";
    std::cout << std::fixed << std::setprecision(2);
    for (const auto& p : ps)
        std::cout << std::left << std::setw(5) << p.id << std::setw(24) << p.name.substr(0, 23) << std::right
                  << std::setw(10) << p.price << std::setw(8) << p.quantity << std::setw(9) << p.reorderLevel
                  << std::setw(7) << p.gstPercent << "  " << (p.isLowStock() ? "LOW STOCK" : "OK") << "\n";
}

static void printBillSummary(const std::vector<Bill>& bills) {
    if (bills.empty()) { std::cout << "No bills yet.\n"; return; }
    std::cout << std::left << std::setw(6) << "Bill" << std::setw(22) << "Customer" << std::setw(21) << "Date"
              << std::right << std::setw(12) << "Total" << "\n" << std::string(61, '-') << "\n"
              << std::fixed << std::setprecision(2);
    for (const auto& b : bills)
        std::cout << std::left << std::setw(6) << b.id << std::setw(22) << b.customer.substr(0, 21)
                  << std::setw(21) << b.createdAt << std::right << std::setw(12) << b.total << "\n";
}

// ---------- menu actions ----------
static void addProduct(Inventory& inv) {
    Product p;
    p.name         = readLine("Name: ");
    p.price        = readDouble("Price: ");
    p.quantity     = readInt("Opening stock: ", 0);
    p.reorderLevel = readInt("Reorder level: ", 0);
    p.gstPercent   = readDouble("GST % (e.g. 18): ", 0, 100);
    std::cout << "Product added with ID " << inv.addProduct(p) << "\n";
}

static void newBill(Inventory& inv, BillingService& billing) {
    std::string customer = readLine("Customer name (blank = Walk-in): ");
    std::vector<CartItem> cart;
    while (true) {
        int id = readInt("Product ID (0 to finish): ", 0);
        if (id == 0) break;
        auto p = inv.find(id);
        if (!p) { std::cout << "  Product not found.\n"; continue; }
        std::cout << "  " << p->name << " | price " << p->price << " | in stock " << p->quantity << "\n";
        if (p->quantity == 0) { std::cout << "  Out of stock.\n"; continue; }
        int qty = readInt("  Quantity: ", 1, p->quantity);
        cart.push_back({id, qty});
    }
    if (cart.empty()) { std::cout << "Cart empty - bill cancelled.\n"; return; }
    double disc = readDouble("Discount % (0 for none): ", 0, 100);

    int billId = billing.createBill(customer, cart, disc);
    std::string path = billing.writeReceipt(billId);
    std::cout << "\nBill #" << billId << " created. Receipt saved to " << path << "\n\n";
    std::ifstream f(path);
    std::cout << f.rdbuf();
}

static void showReport(BillingService& billing, Inventory& inv) {
    SalesReport r = billing.salesReport();
    std::cout << std::fixed << std::setprecision(2)
              << "Total bills     : " << r.billCount << "\n"
              << "Total revenue   : " << r.totalRevenue << "\n"
              << "Today's revenue : " << r.todayRevenue << "\n\nTop selling products:\n";
    if (r.topProducts.empty()) std::cout << "  (no sales yet)\n";
    for (const auto& [name, units] : r.topProducts) std::cout << "  " << std::left << std::setw(24) << name << units << " units\n";
    auto low = inv.lowStock();
    std::cout << "\nLow-stock items: " << low.size() << "\n";
}

static const char* kMenu = R"(
===== INVENTORY & BILLING MANAGEMENT SYSTEM =====
 1. Add product            7. Low-stock alert
 2. List products          8. New bill
 3. Search product         9. View / reprint bill
 4. Update stock          10. Recent bills
 5. Update price          11. Sales report
 6. Delete product        12. Export products to CSV
 0. Exit
)";

int main(int argc, char** argv) {
    try {
        Database db("inventory.db");
        Inventory inv(db);
        BillingService billing(db, inv);
        if (argc > 1 && std::string(argv[1]) == "--demo") {
            inv.seedDemoData();
            std::cout << "Demo data loaded (only if the product table was empty).\n";
        }

        while (true) {
            std::cout << kMenu;
            int choice = readInt("Choice: ", 0, 12);
            if (choice == 0) break;
            try {
                switch (choice) {
                    case 1:  addProduct(inv); break;
                    case 2:  printProducts(inv.listAll()); break;
                    case 3:  printProducts(inv.search(readLine("Search keyword: "))); break;
                    case 4: {
                        int id = readInt("Product ID: ", 1);
                        int delta = readInt("Change in stock (+ add, - remove): ");
                        inv.adjustStock(id, delta);
                        std::cout << "Stock updated.\n";
                        break;
                    }
                    case 5: {
                        int id = readInt("Product ID: ", 1);
                        inv.updatePrice(id, readDouble("New price: "));
                        std::cout << "Price updated.\n";
                        break;
                    }
                    case 6: {
                        int id = readInt("Product ID: ", 1);
                        if (readLine("Type YES to confirm delete: ") == "YES") {
                            inv.removeProduct(id);
                            std::cout << "Deleted.\n";
                        } else std::cout << "Cancelled.\n";
                        break;
                    }
                    case 7:  printProducts(inv.lowStock()); break;
                    case 8:  newBill(inv, billing); break;
                    case 9: {
                        int id = readInt("Bill number: ", 1);
                        std::ifstream f(billing.writeReceipt(id));
                        std::cout << f.rdbuf();
                        break;
                    }
                    case 10: printBillSummary(billing.recentBills()); break;
                    case 11: showReport(billing, inv); break;
                    case 12: inv.exportCsv("products_export.csv"); std::cout << "Saved products_export.csv\n"; break;
                }
            } catch (const InputClosed&) {
                throw;
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << "\n";
            }
        }
    } catch (const InputClosed&) {
        // stdin closed - exit quietly
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    std::cout << "Bye.\n";
    return 0;
}