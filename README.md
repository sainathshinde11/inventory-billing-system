# Inventory & Billing Management System

Console application in **C++17** using **OOP**, **SQL (SQLite)** and **file handling**.

## Features
- Product CRUD, search, price/stock updates, low-stock alerts
- Multi-item billing with GST, discount and stock validation
- **Atomic billing**: stock deduction + bill + items run in one SQL transaction (rollback on any failure)
- **No overselling**: `UPDATE ... WHERE quantity >= ?` guarantees stock never goes negative
- Receipts written to `receipts/bill_<id>.txt`; reprint any past bill
- Inventory export to CSV
- Sales report: bill count, total/today revenue, top-selling products

## Design
| File | Responsibility |
|---|---|
| Database.h/.cpp | RAII wrappers for SQLite connection and prepared statements |
| Product.h | Product entity + validation |
| Inventory.h/.cpp | Stock and product operations |
| Billing.h/.cpp | Billing transaction, receipts, reports |
| main.cpp | Menu UI and validated input |

Prepared statements are used everywhere (no SQL injection). Bill items store a snapshot of
name/price/GST, so old bills stay correct after prices change or products are deleted.

## Build
**Windows (MinGW-w64):** download the SQLite amalgamation from sqlite.org, copy `sqlite3.c` and
`sqlite3.h` into `lib\`, then:
```bat
build.bat
inventory.exe --demo
```
**Linux/macOS:** `sudo apt install libsqlite3-dev && make && ./inventory --demo`

## Tests
`make test` (Linux/macOS) runs `tests/test_billing.cpp`: bill totals, stock deduction, full rollback when a
multi-item bill fails, input validation, and bills surviving product deletion.