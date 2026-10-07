// Minimal self-checking tests (no framework). Run: make test
#include <cassert>
#include <cmath>
#include <iostream>
#include "../src/Billing.h"

static bool near(double a, double b) { return std::fabs(a - b) < 0.01; }

int main() {
    Database db(":memory:");
    Inventory inv(db);
    BillingService billing(db, inv);

    int pen  = inv.addProduct({0, "Pen",  10.0, 10, 2, 12.0});
    int lamp = inv.addProduct({0, "Lamp", 500.0, 3, 1, 18.0});

    // 1. Normal bill: totals and stock deduction
    int b1 = billing.createBill("Test", {{pen, 4}, {lamp, 1}}, 10);
    Bill bill = billing.getBill(b1);
    assert(near(bill.subtotal, 540.0));
    assert(near(bill.discount, 54.0));
    assert(near(bill.gst, (4*10*0.12 + 500*0.18) * 0.9));
    assert(near(bill.total, 540 - 54 + bill.gst));
    assert(inv.find(pen)->quantity == 6 && inv.find(lamp)->quantity == 2);

    // 2. Failure midway must roll back EVERYTHING (stock of 'pen' restored, no new bill)
    bool threw = false;
    try { billing.createBill("Test", {{pen, 2}, {lamp, 99}}, 0); }
    catch (const std::runtime_error&) { threw = true; }
    assert(threw);
    assert(inv.find(pen)->quantity == 6);                 // unchanged
    assert(billing.recentBills(10).size() == 1);          // no partial bill saved

    // 3. Validation
    threw = false; try { billing.createBill("x", {}, 0); } catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
    threw = false; try { billing.createBill("x", {{pen, 1}}, 150); } catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
    threw = false; try { inv.addProduct({0, "Bad", -5, 1, 1, 18}); } catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
    threw = false; try { inv.adjustStock(pen, -100); } catch (const std::runtime_error&) { threw = true; }
    assert(threw);

    // 4. Old bill survives product deletion
    inv.removeProduct(lamp);
    assert(billing.getBill(b1).lines.size() == 2);

    std::cout << "All tests passed.\n";
    return 0;
}