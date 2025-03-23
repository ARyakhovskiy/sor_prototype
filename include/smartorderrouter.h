#ifndef SMARTORDERROUTER_H
#define SMARTORDERROUTER_H

#include "orderbook.h"
#include <unordered_map>
#include <queue>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <limits>
#include <iostream>
#include <iomanip>

struct BestOrder {
    std::string exchange_name;
    Price effective_price; // Price after fees
    Volume volume;
    Price original_price;  // Original price before fees
    double fee;             // Taker fee for the exchange

    // Comparator for buy orders (lowest effective price first)
    struct BuyComparator 
    {
        bool operator()(const BestOrder& a, const BestOrder& b) const {
            return a.effective_price > b.effective_price; // Min-heap for buy orders
        }
    };

    // Comparator for sell orders (highest effective price first)
    struct SellComparator 
    {
        bool operator()(const BestOrder& a, const BestOrder& b) const {
            return a.effective_price < b.effective_price; // Max-heap for sell orders
        }
    };
};

class SmartOrderRouter {
private:
    std::unordered_map<std::string, std::shared_ptr<OrderBook>> order_books; // Maps exchange name to OrderBook

public:
    SmartOrderRouter(const std::unordered_map<std::string, std::shared_ptr<OrderBook>>& order_books);

    // Function to distribute an order across exchanges
    std::vector<std::pair<std::string, std::pair<double, double>>> distribute_order(double order_size, bool is_buy) const;
   
};

#endif // SMARTORDERROUTER_H