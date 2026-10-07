#pragma once
#include <string>
#include <vector>
#include "Database.h"
#include "Inventory.h"

struct CartItem { int productId; int quantity; };

struct BillLine {
    int         productId = 0;
    std::string name;
    int         quantity = 0;
    double      unitPrice = 0;
    double      gstPercent = 0;
    double      lineTotal = 0;      // quantity * unitPrice (before GST)
};

struct Bill {
    int         id = 0;
    std::string customer;
    std::string createdAt;
    double      subtotal = 0, discount = 0, gst = 0, total = 0;
    std::vector<BillLine> lines;
};

struct SalesReport {
    int    billCount = 0;
    double totalRevenue = 0;
    double todayRevenue = 0;
    std::vector<std::pair<std::string, int>> topProducts;   // name, units sold
};

class BillingService {
public:
    BillingService(Database& db, Inventory& inv) : db_(db), inv_(inv) {}

    // Atomic: stock deduction + bill + bill items succeed together or not at all.
    int  createBill(const std::string& customer, const std::vector<CartItem>& cart, double discountPct);
    Bill getBill(int id);
    std::vector<Bill> recentBills(int limit = 10);
    SalesReport salesReport();

    // File handling: writes receipts/bill_<id>.txt and returns the path
    std::string writeReceipt(int billId, const std::string& dir = "receipts");

private:
    Database& db_;
    Inventory& inv_;
};