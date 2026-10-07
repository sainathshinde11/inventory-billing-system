#include "Inventory.h"
#include <fstream>

static const char* kSelect =
    "SELECT id, name, price, quantity, reorder_level, gst_percent FROM products ";

Product Inventory::fromRow(Statement& st) {
    Product p;
    p.id           = st.getInt(0);
    p.name         = st.getText(1);
    p.price        = st.getDouble(2);
    p.quantity     = st.getInt(3);
    p.reorderLevel = st.getInt(4);
    p.gstPercent   = st.getDouble(5);
    return p;
}

void Inventory::requireExists(int id) {
    if (!find(id)) throw std::runtime_error("Product " + std::to_string(id) + " not found");
}

int Inventory::addProduct(const Product& p) {
    p.validate();
    Statement st(db_, "INSERT INTO products (name, price, quantity, reorder_level, gst_percent) "
                      "VALUES (?, ?, ?, ?, ?)");
    st.bindText(1, p.name).bindDouble(2, p.price).bindInt(3, p.quantity)
      .bindInt(4, p.reorderLevel).bindDouble(5, p.gstPercent);
    st.run();
    return db_.lastInsertId();
}

std::optional<Product> Inventory::find(int id) {
    Statement st(db_, std::string(kSelect) + "WHERE id = ?");
    st.bindInt(1, id);
    if (st.step()) return fromRow(st);
    return std::nullopt;
}

std::vector<Product> Inventory::listAll() {
    std::vector<Product> out;
    Statement st(db_, std::string(kSelect) + "ORDER BY id");
    while (st.step()) out.push_back(fromRow(st));
    return out;
}

std::vector<Product> Inventory::search(const std::string& keyword) {
    std::vector<Product> out;
    Statement st(db_, std::string(kSelect) + "WHERE name LIKE ? ORDER BY name");
    st.bindText(1, "%" + keyword + "%");
    while (st.step()) out.push_back(fromRow(st));
    return out;
}

std::vector<Product> Inventory::lowStock() {
    std::vector<Product> out;
    Statement st(db_, std::string(kSelect) + "WHERE quantity <= reorder_level ORDER BY quantity");
    while (st.step()) out.push_back(fromRow(st));
    return out;
}

void Inventory::adjustStock(int id, int delta) {
    auto p = find(id);
    if (!p) throw std::runtime_error("Product " + std::to_string(id) + " not found");
    if (p->quantity + delta < 0)
        throw std::runtime_error("Stock cannot go below zero (current: " + std::to_string(p->quantity) + ")");
    Statement st(db_, "UPDATE products SET quantity = quantity + ? WHERE id = ?");
    st.bindInt(1, delta).bindInt(2, id);
    st.run();
}

void Inventory::updatePrice(int id, double newPrice) {
    if (newPrice < 0) throw std::invalid_argument("Price cannot be negative");
    requireExists(id);
    Statement st(db_, "UPDATE products SET price = ? WHERE id = ?");
    st.bindDouble(1, newPrice).bindInt(2, id);
    st.run();
}

void Inventory::removeProduct(int id) {
    requireExists(id);
    Statement st(db_, "DELETE FROM products WHERE id = ?");
    st.bindInt(1, id);
    st.run();
}

void Inventory::seedDemoData() {
    Statement cnt(db_, "SELECT COUNT(*) FROM products");
    cnt.step();
    if (cnt.getInt(0) > 0) return;
    const Product demo[] = {
        {0, "Notebook A4",      45.0, 100, 20, 12.0},
        {0, "Ball Pen (Blue)",  10.0, 250, 50, 12.0},
        {0, "USB Flash Drive",  399.0, 30, 10, 18.0},
        {0, "Wireless Mouse",   649.0, 15,  5, 18.0},
        {0, "Desk Lamp",        899.0,  8,  5, 18.0},
        {0, "Stapler",          120.0,  4,  5, 18.0},
    };
    for (const auto& p : demo) addProduct(p);
}

void Inventory::exportCsv(const std::string& path) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("Cannot write file: " + path);
    out << "id,name,price,quantity,reorder_level,gst_percent\n";
    for (const auto& p : listAll()) {
        std::string safe = p.name;
        for (size_t pos = 0; (pos = safe.find('"', pos)) != std::string::npos; pos += 2)
            safe.insert(pos, "\"");
        out << p.id << ",\"" << safe << "\"," << p.price << ',' << p.quantity << ','
            << p.reorderLevel << ',' << p.gstPercent << '\n';
    }
}