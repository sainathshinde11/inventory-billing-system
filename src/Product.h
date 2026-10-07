#pragma once
#include <string>
#include <stdexcept>

struct Product {
    int         id = 0;
    std::string name;
    double      price = 0.0;
    int         quantity = 0;
    int         reorderLevel = 5;
    double      gstPercent = 18.0;

    void validate() const {
        if (name.empty())            throw std::invalid_argument("Product name cannot be empty");
        if (price < 0)               throw std::invalid_argument("Price cannot be negative");
        if (quantity < 0)            throw std::invalid_argument("Quantity cannot be negative");
        if (reorderLevel < 0)        throw std::invalid_argument("Reorder level cannot be negative");
        if (gstPercent < 0 || gstPercent > 100)
                                     throw std::invalid_argument("GST must be between 0 and 100");
    }
    bool isLowStock() const { return quantity <= reorderLevel; }
};