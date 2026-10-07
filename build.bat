@echo off
REM Requires g++/gcc (MinGW-w64) and lib\sqlite3.c + lib\sqlite3.h (SQLite amalgamation)
if not exist build mkdir build
if not exist build\sqlite3.o (
    gcc -O2 -c lib\sqlite3.c -o build\sqlite3.o || exit /b 1
)
g++ -std=c++17 -O2 -Wall -Wextra -Ilib src\main.cpp src\Database.cpp src\Inventory.cpp src\Billing.cpp build\sqlite3.o -o inventory.exe || exit /b 1
echo Build OK  ^>  run: inventory.exe --demo