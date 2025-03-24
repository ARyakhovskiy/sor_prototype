#include "smartorderrouter.h"
#include <unordered_set>

constexpr double EPSILON = 1e-12;

SmartOrderRouter::SmartOrderRouter(std::unordered_map<ExchangeName, std::shared_ptr<OrderBook>> order_books)
    : order_books(std::make_unique<std::unordered_map<ExchangeName, std::shared_ptr<OrderBook>>>(std::move(order_books))) {}

Price effective_price(Price original_price, bool is_buy, double fee)
{
    return is_buy ? original_price * (1 + fee) : original_price * (1 - fee);
}

double SmartOrderRouter::get_largest_min_lot_size(
    const std::priority_queue<BestOrder, std::vector<BestOrder>, Comparator>& best_orders) const
{
    double largest_min = 0.0;
    auto queue_copy = best_orders; // Makes a copy
    
    std::unordered_set<std::string> seen_exchanges;
    while (!queue_copy.empty()) {
        const auto& order = queue_copy.top();
        if (seen_exchanges.insert(order.exchange_name).second) {
            double min_size = order_books->at(order.exchange_name)->get_min_order_size();
            largest_min = std::max(largest_min, min_size);
        }
        queue_copy.pop();
    }
    return largest_min;
}

ExecutionPlan SmartOrderRouter::distribute_order(double order_size, bool is_buy) const
{
    std::vector<FillOrder> execution_plan;
    Volume remaining_size = order_size;
    double total_fees = 0.0;

    using Comparator = std::function<bool(const BestOrder&, const BestOrder&)>;
    Comparator comparator;
    if (is_buy) 
    {
        comparator = BestOrder::BuyComparator();
    } 
    else
    {
        comparator = BestOrder::SellComparator();
    }

    std::priority_queue<BestOrder, std::vector<BestOrder>, Comparator> best_orders(comparator);
    std::cout << "Initial Order: Size = " << order_size << ", Type = " << (is_buy ? "Buy" : "Sell") << std::endl;

    Volume absoluteMinLotSize = order_size;

    // Initialize the priority queue with the best orders from each exchange
    for (const auto& [exchange_name, order_book] : *order_books) 
    {
        const auto& order_side = is_buy ? order_book->get_asks() : order_book->get_bids();

        if (!order_side.empty()) 
        {
            auto best_order = is_buy ? order_side.begin() : std::prev(order_side.end());
            Price price = best_order->first;
            Volume volume = best_order->second;
            double fee = order_book->get_taker_fee();
            
            absoluteMinLotSize = std::min(absoluteMinLotSize, order_book->get_min_order_size());

            // Add to the priority queue
            best_orders.push({exchange_name, effective_price(price, is_buy, fee), volume, price, fee});

            // Debug output: Print the order added to the priority queue
            std::cout << "Added order to queue: Exchange = " << exchange_name
                      << ", Effective Price = " << effective_price(price, is_buy, fee)
                      << ", Volume = " << volume
                      << ", Original Price = " << price
                      << ", Fee = " << fee << std::endl;
        }
    }

    while (remaining_size >= absoluteMinLotSize && !best_orders.empty()) 
    {
        // Debug output: Print the current state of the priority queue
        std::cout << "\nCurrent state of best_orders queue:" << std::endl;
        auto temp_queue = best_orders; // Copy the queue for debugging
        while (!temp_queue.empty()) 
        {
            auto order = temp_queue.top();
            std::cout << "Exchange = " << order.exchange_name
                      << ", Effective Price = " << order.effective_price
                      << ", Volume = " << order.volume
                      << ", Original Price = " << order.original_price
                      << ", Fee = " << order.fee << std::endl;
            temp_queue.pop();
        }

        // Get the best order (top of the priority queue)
        auto best_order = best_orders.top();
        best_orders.pop(); // Remove the fulfilled order from the priority queue

        // Debug output: Print the order being processed
        std::cout << "\nProcessing order: Exchange = " << best_order.exchange_name
                  << ", Effective Price = " << best_order.effective_price
                  << ", Volume = " << best_order.volume
                  << ", Original Price = " << best_order.original_price
                  << ", Fee = " << best_order.fee << std::endl;

        // Calculate the quantity to take from this exchange
        double fill_quantity = std::min(best_order.volume, remaining_size);
        // Ensure the fill quantity is a multiple of the minimum order size
        double min_order_size = order_books->at(best_order.exchange_name)->get_min_order_size();

        fill_quantity = std::floor((fill_quantity / min_order_size) + EPSILON) * min_order_size;

        if (fill_quantity > 0) 
        {
            // Add to execution plan (use the original price, not the effective price)
            execution_plan.push_back({best_order.exchange_name, {best_order.original_price, fill_quantity}});

            std::cout << "Added to execution plan: Exchange = " << best_order.exchange_name
                      << ", Price = " << best_order.original_price
                      << ", Quantity = " << fill_quantity << std::endl;

            double fees = fill_quantity * best_order.original_price * best_order.fee;
            total_fees += fees;
            std::cout << "Fees for this fill: " << fees << std::endl;
            remaining_size -= fill_quantity;
            std::cout << "Remaining size to fill: " << remaining_size << std::endl;
        }
        else 
        {
            std::cout << "Skipping order from " << best_order.exchange_name
                      << " because fill_quantity <= 0." << std::endl;
            std::cout << "Remaining size to fill: " << remaining_size << std::endl;
        }

        // Remove the fulfilled/skipped order from the exchange's order book
        auto& order_book = order_books->at(best_order.exchange_name);
        if (is_buy) 
        {
            order_book->remove_top_ask();
        } 
        else 
        {
            order_book->remove_top_bid();
        }

        // Add the next best order from the same exchange (if available)
        const auto& order_side = is_buy ? order_book->get_asks() : order_book->get_bids();
        if (!order_side.empty() && order_book->get_min_order_size() <= remaining_size) 
        {
            auto next_order = is_buy ? order_side.begin() : std::prev(order_side.end());
            Price price = next_order->first;
            Volume volume = next_order->second;
            double fee = order_book->get_taker_fee();

            Price eff_price = effective_price(price, is_buy, fee);
            best_orders.push({best_order.exchange_name, eff_price, volume, price, fee});
            std::cout << "Added next order to queue: Exchange = " << best_order.exchange_name
                      << ", Effective Price = " << eff_price
                      << ", Volume = " << volume
                      << ", Original Price = " << price
                      << ", Fee = " << fee << std::endl;
        }        
    }

    // Print total fees
    std::cout << "Total Fees Paid: " << std::fixed << std::setprecision(2) << total_fees << std::endl;

    // Create a shared_ptr that aliases the unique_ptr's data
    auto order_books_ptr = std::shared_ptr<const std::unordered_map<ExchangeName, std::shared_ptr<OrderBook>>>(
        order_books.get(),
        [](auto*) {} // No-op deleter since unique_ptr maintains ownership
    );

    return ExecutionPlan(execution_plan, order_books_ptr, is_buy, order_size);
}