#ifndef EXECUTION_PLAN_H
#define EXECUTION_PLAN_H

#include <vector>
#include <string>
#include <utility>
#include <unordered_map>
#include <memory>
#include <iomanip>
#include <iostream>
#include "orderbook.h"

class ExecutionPlan {
private:
    std::vector<std::pair<std::string, std::pair<Price, Volume>>> plan;
    std::unordered_map<std::string, std::shared_ptr<class OrderBook>> order_books;
    bool is_buy;
    Volume original_order_size;

public:
    // Constructor
    ExecutionPlan(const std::vector<std::pair<std::string, std::pair<Price, Volume>>>& plan,
                const std::unordered_map<std::string, std::shared_ptr<class OrderBook>>& order_books,
                bool is_buy,
                double original_order_size);

    const std::vector<std::pair<std::string, std::pair<Price, Volume>>>& get_plan() const;
    Price get_total_fees() const;

    // Compute and get total cost (for buy orders) or total profit (for sell orders)
    Price get_total() const;

    Price get_average_effective_price() const;
    double get_fulfillment_percentage() const;

    void print() const;
};

#endif // EXECUTION_PLAN_H