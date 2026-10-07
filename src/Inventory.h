#pragma once
#include <optional>
#include <string>
#include <vector>
#include "Database.h"
#include "Product.h"

// Product / stock management.
class Inventory {
public:
    explicit Inventory(Database& db) : db_(db) {}

    int  addProduct(const Product& p);
    std::optional<Product> find(int id);
    std::vector<Product> listAll();
    std::vector<Product> search(const std::string& keyword);
    std::vector<Product> lowStock();

    void adjustStock(int id, int delta);       // +ve restock, -ve write-off
    void updatePrice(int id, double newPrice);
    void removeProduct(int id);
    void seedDemoData();                       // only if table is empty

    // File handling: export inventory snapshot to CSV
    void exportCsv(const std::string& path);

private:
    Database& db_;
    static Product fromRow(Statement& st);
    void requireExists(int id);
};