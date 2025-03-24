#ifndef SMARTORDERROUTER_H
#define SMARTORDERROUTER_H

#include "executionplan.h"
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
    ExchangeName exchange_name;
    Price effective_price;
    Volume volume;
    Price original_price; 
    double fee;             

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

class SmartOrderRouter 
{
private:
    std::unordered_map<ExchangeName, std::shared_ptr<OrderBook>> order_books; // Maps exchange name to OrderBook;

public:
    // Constructor
    SmartOrderRouter(std::unordered_map<std::string, std::shared_ptr<OrderBook>> order_books);

    // Move constructor
    SmartOrderRouter(SmartOrderRouter&& other) noexcept;
    // Greedy distribution method
    ExecutionPlan distribute_order(Volume order_size, bool is_buy) const;
    // Dynamic programming order distribution method
    ExecutionPlan distribute_order_dp(Volume order_size, bool is_buy) const;
};

#endif // SMARTORDERROUTER_H