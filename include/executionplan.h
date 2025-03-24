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

using FillOrder = std::pair<ExchangeName, std::pair<Price, Volume>>;

class ExecutionPlan 
{
private:
    std::vector<FillOrder> m_plan;
    std::shared_ptr<const std::unordered_map<ExchangeName, std::shared_ptr<class OrderBook>>> m_order_books;
    OrderSide m_side;
    Volume m_original_order_size;

public:
    // Constructor
    ExecutionPlan(const std::vector<FillOrder>& plan,
                std::shared_ptr<const std::unordered_map<ExchangeName, std::shared_ptr<class OrderBook>>> order_books,
                OrderSide side,
                Volume original_order_size);

    void add_fill(const ExchangeName& exchange_name, Price price, Volume quantity);

    const std::vector<FillOrder>& get_plan() const;
    Price get_total_fees() const;

    // Compute and get total cost (for buy orders) or total profit (for sell orders)
    Price get_total() const;

    Price get_average_effective_price() const;
    double get_fulfillment_percentage() const;

    void print() const;
};

#endif // EXECUTION_PLAN_H