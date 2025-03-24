#include "executionplan.h"

ExecutionPlan::ExecutionPlan(const std::vector<FillOrder>& plan,
                            std::shared_ptr<const std::unordered_map<ExchangeName, std::shared_ptr<OrderBook>>> order_books,
                            OrderSide side,
                            Volume original_order_size
) : m_plan(plan), m_order_books(std::move(order_books)), m_side(side), m_original_order_size(original_order_size) {}

const std::vector<FillOrder>& ExecutionPlan::get_plan() const 
{
    return m_plan;
}

void ExecutionPlan::add_fill(FillOrder fill)
{
    m_plan.emplace_back(fill);
}

Price ExecutionPlan::get_total_fees() const 
{
    double total_fees = 0.0;
    for (const auto& record : m_plan) 
    {
        const ExchangeName& exchange_name = record.exchange_name;
        Price price = record.price;
        Volume quantity = record.volume;
        double fee = m_order_books->at(exchange_name)->get_taker_fee();
        total_fees += quantity * price * fee;
    }
    return total_fees;
}

// Get total cost (for buy orders) or total profit (for sell orders)
Price ExecutionPlan::get_total() const 
{
    Price total = 0.0;
    for (const FillOrder& record : m_plan) 
    {
        const ExchangeName& exchange_name = record.exchange_name;
        Price price = record.price;
        Volume quantity = record.volume;
        double fee = m_order_books->at(exchange_name)->get_taker_fee();
        Price effective_price = (m_side == OrderSide::BUY) ? price * (1 + fee) : price * (1 - fee);
        total += quantity * effective_price;
    }
    return total;
}

Price ExecutionPlan::get_average_effective_price() const 
{
    Volume total_quantity = 0.0;
    for (const FillOrder& record : m_plan) 
    {
        total_quantity += record.volume;
    }
    if (total_quantity == 0.0) 
    {
        return 0.0;
    }
    return get_total() / total_quantity;
}

double ExecutionPlan::get_fulfillment_percentage() const 
{
    if (m_original_order_size == 0)
    {
        return 100.0;
    }
    else
    {
        Volume total_quantity = 0.0;
        for (const auto& record : m_plan) 
        {
            total_quantity += record.volume;
        }
        return (total_quantity / m_original_order_size) * 100.0;
    }
}

void ExecutionPlan::print() const 
{
    std::cout << "Execution Plan:" << std::endl;
    for (const auto& record : m_plan) 
    {
        const ExchangeName& exchange_name = record.exchange_name;
        Price price = record.price;
        Volume quantity = record.volume;
        double fee_rate = m_order_books->at(exchange_name)->get_taker_fee();
        Price fee_amount = quantity * price * fee_rate; // Actual fee amount
        Price effective_price = (m_side == OrderSide::BUY) ? price * (1 + fee_rate) : price * (1 - fee_rate);

        std::cout << "Exchange: " << exchange_name
                  << ", Price: " << std::fixed << std::setprecision(2) << price
                  << ", Quantity: " << std::fixed << std::setprecision(5) << quantity
                  << ", Fee Amount: " << std::fixed << std::setprecision(2) << fee_amount
                  << ", Effective Price: " << effective_price << std::endl;
    }

    std::cout << "\nMetrics:" << std::endl;
    std::cout << "Total Fees: " << get_total_fees() << std::endl;
    if (m_side == OrderSide::BUY) 
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