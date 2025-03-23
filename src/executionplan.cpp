#include "../include/executionplan.h"

ExecutionPlan::ExecutionPlan(
    const std::vector<std::pair<std::string, std::pair<Price, Volume>>>& plan,
    const std::unordered_map<std::string, std::shared_ptr<OrderBook>>& order_books,
    bool is_buy,
    Volume original_order_size
) : plan(plan), order_books(order_books), is_buy(is_buy), original_order_size(original_order_size) {}

const std::vector<std::pair<std::string, std::pair<Price, Volume>>>& ExecutionPlan::get_plan() const 
{
    return plan;
}

Price ExecutionPlan::get_total_fees() const 
{
    double total_fees = 0.0;
    for (const auto& record : plan) {
        const std::string& exchange_name = record.first;
        Price price = record.second.first;
        Volume quantity = record.second.second;
        double fee = order_books.at(exchange_name)->get_taker_fee();
        total_fees += quantity * price * fee;
    }
    return total_fees;
}

// Get total cost (for buy orders) or total profit (for sell orders)
Price ExecutionPlan::get_total() const 
{
    Price total = 0.0;
    for (const auto& record : plan) 
    {
        const std::string& exchange_name = record.first;
        Price price = record.second.first;
        Volume quantity = record.second.second;
        double fee = order_books.at(exchange_name)->get_taker_fee();
        Price effective_price = is_buy ? price * (1 + fee) : price * (1 - fee);
        total += quantity * effective_price;
    }
    return total;
}

// Compute and get average effective price
Price ExecutionPlan::get_average_effective_price() const 
{
    Volume total_quantity = 0.0;
    for (const auto& record : plan) 
    {
        total_quantity += record.second.second;
    }
    if (total_quantity == 0.0) 
    {
        return 0.0;
    }
    return get_total() / total_quantity;
}

double ExecutionPlan::get_fulfillment_percentage() const 
{
    if (original_order_size == 0)
    {
        return 100;
    }
    else
    {
        Volume total_quantity = 0.0;
        for (const auto& record : plan) 
        {
            total_quantity += record.second.second;
        }
        return (total_quantity / original_order_size) * 100.0;
    }
}

// Print the execution plan and metrics
void ExecutionPlan::print() const 
{
    std::cout << "Execution Plan:" << std::endl;
    for (const auto& record : plan) 
    {
        const std::string& exchange_name = record.first;
        Price price = record.second.first;
        Volume quantity = record.second.second;
        double fee_rate = order_books.at(exchange_name)->get_taker_fee();
        Price fee_amount = quantity * price * fee_rate; // Actual fee amount
        Price effective_price = is_buy ? price * (1 + fee_rate) : price * (1 - fee_rate);

        std::cout << "Exchange: " << exchange_name
                  << ", Price: " << std::fixed << std::setprecision(2) << price
                  << ", Quantity: " << std::fixed << std::setprecision(5) << quantity
                  << ", Fee Amount: " << std::fixed << std::setprecision(2) << price
                  << ", Effective Price: " << effective_price << std::endl;
    }

    std::cout << "\nMetrics:" << std::endl;
    std::cout << "Total Fees: " << get_total_fees() << std::endl;
    if (is_buy) 
    {
        std::cout << "Total Cost (including fees): " << get_total() << std::endl;
    } 
    else 
    {
        std::cout << "Total Profit (after fees): " << get_total() << std::endl;
    }
    std::cout << "Average Effective Price: " << get_average_effective_price() << std::endl;
    std::cout << "Fulfillment Percentage: " << get_fulfillment_percentage() << "%" << std::endl;
}