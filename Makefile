# Linux/macOS: needs libsqlite3-dev (sudo apt install libsqlite3-dev)
CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
SRC = src/main.cpp src/Database.cpp src/Inventory.cpp src/Billing.cpp

inventory: $(SRC) src/*.h
	$(CXX) $(CXXFLAGS) $(SRC) -lsqlite3 -o inventory

clean:
	rm -f inventory inventory.db

test: tests/test_billing.cpp src/Database.cpp src/Inventory.cpp src/Billing.cpp src/*.h
	$(CXX) $(CXXFLAGS) tests/test_billing.cpp src/Database.cpp src/Inventory.cpp src/Billing.cpp -lsqlite3 -o test_billing
	./test_billing