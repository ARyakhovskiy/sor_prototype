#include "smartorderrouter.h"
#include <unordered_set>

constexpr double EPSILON = 1e-12;

SmartOrderRouter::SmartOrderRouter(std::unordered_map<ExchangeName, std::shared_ptr<OrderBook>> order_books)
    : m_order_books(std::make_unique<std::unordered_map<ExchangeName, std::shared_ptr<OrderBook>>>(std::move(order_books))) {}

Price effective_price(Price original_price, bool is_buy, double fee)
{
    return is_buy ? original_price * (1 + fee) : original_price * (1 - fee);
}

double SmartOrderRouter::get_largest_min_lot_size(
    const std::priority_queue<BestOrder, std::vector<BestOrder>, Comparator>& best_orders) const
{
    double largest_min = 0.0;
    auto queue_copy = best_orders;
    
    std::unordered_set<ExchangeName> seen_exchanges;
    while (!queue_copy.empty()) {
        const auto& order = queue_copy.top();
        if (seen_exchanges.insert(order.exchange_name).second) {
            double min_size = m_order_books->at(order.exchange_name)->get_min_order_size();
            largest_min = std::max(largest_min, min_size);
        }
        queue_copy.pop();
    }
    return largest_min;
}

ExecutionPlan SmartOrderRouter::distribute_order(Volume order_size, bool is_buy) const
{
    // Create aliasing shared_ptr for order books
    auto order_books_ptr = std::shared_ptr<const std::unordered_map<ExchangeName, std::shared_ptr<OrderBook>>>(
        m_order_books.get(),
        [](auto*) {} // No-op deleter
    );
    
    // Create empty execution plan at start
    ExecutionPlan execution_plan({}, order_books_ptr, is_buy, order_size);

    Volume remaining_size = order_size;
    Volume absoluteMinLotSize = order_size;

    // Initialize priority queue with original comparator logic
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

    // Initialize priority queue with best orders from each exchange
    for (const auto& [exchange_name, order_book] : *m_order_books) 
    {
        const auto& order_side = is_buy ? order_book->get_asks() : order_book->get_bids();

        if (!order_side.empty()) 
        {
            auto best_order = is_buy ? order_side.begin() : --order_side.end();
            Price price = best_order->first;
            Volume volume = best_order->second;
            double fee = order_book->get_taker_fee();
            
            absoluteMinLotSize = std::min(absoluteMinLotSize, order_book->get_min_order_size());

            best_orders.push({
                exchange_name,
                effective_price(price, is_buy, fee),
                volume,
                price,
                fee
            });

            std::cout << "Added order to queue: Exchange = " << exchange_name
                      << ", Effective Price = " << effective_price(price, is_buy, fee)
                      << ", Volume = " << volume
                      << ", Original Price = " << price
                      << ", Fee = " << fee << std::endl;
        }
    }

    while (remaining_size >= absoluteMinLotSize && !best_orders.empty()) 
    {
        std::cout << "\nCurrent state of best_orders queue:" << std::endl;
        auto temp_queue = best_orders;
        while (!temp_queue.empty()) 
        {
            const auto& order = temp_queue.top();
            std::cout << "Exchange = " << order.exchange_name
                      << ", Effective Price = " << order.effective_price
                      << ", Volume = " << order.volume
                      << ", Original Price = " << order.original_price
                      << ", Fee = " << order.fee << std::endl;
            temp_queue.pop();
        }

        auto best_order = best_orders.top();
        best_orders.pop();

        std::cout << "\nProcessing order: Exchange = " << best_order.exchange_name
                  << ", Effective Price = " << best_order.effective_price
                  << ", Volume = " << best_order.volume
                  << ", Original Price = " << best_order.original_price
                  << ", Fee = " << best_order.fee << std::endl;

        Volume fill_quantity = std::min(best_order.volume, remaining_size);
        Volume min_order_size = m_order_books->at(best_order.exchange_name)->get_min_order_size();
        fill_quantity = std::floor((fill_quantity / min_order_size) + EPSILON) * min_order_size;

        if (fill_quantity > 0) 
        {
            execution_plan.add_fill(best_order.exchange_name, best_order.original_price, fill_quantity);

            std::cout << "Added to execution plan: Exchange = " << best_order.exchange_name
                      << ", Price = " << best_order.original_price
                      << ", Quantity = " << fill_quantity << std::endl;

            Price fees = fill_quantity * best_order.original_price * best_order.fee;
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

        // Update order book
        auto& order_book = m_order_books->at(best_order.exchange_name);
        if (is_buy) {
            order_book->remove_top_ask();
        } else {
            order_book->remove_top_bid();
        }

        // Add next order from same exchange if available
        const auto& order_side = is_buy ? order_book->get_asks() : order_book->get_bids();
        if (!order_side.empty() && order_book->get_min_order_size() <= remaining_size) 
        {
            auto next_order = is_buy ? order_side.begin() : --order_side.end();
            Price price = next_order->first;
            Volume volume = next_order->second;
            double fee = order_book->get_taker_fee();

            best_orders.push({
                best_order.exchange_name,
                effective_price(price, is_buy, fee),
                volume,
                price,
                fee
            });

            std::cout << "Added next order to queue: Exchange = " << best_order.exchange_name
                      << ", Effective Price = " << effective_price(price, is_buy, fee)
                      << ", Volume = " << volume
                      << ", Original Price = " << price
                      << ", Fee = " << fee << std::endl;
        }        
    }

    std::cout << "Total Fees Paid: " << std::fixed << std::setprecision(2) << execution_plan.get_total_fees() << std::endl;

    return execution_plan;
}

void SmartOrderRouter::print_remaining_liquidity() const
{
    std::cout << "\n=== Remaining Liquidity Across Exchanges ===" << std::endl;
    
    double total_buy_liquidity = 0.0;
    double total_sell_liquidity = 0.0;
    size_t total_buy_levels = 0;
    size_t total_sell_levels = 0;

    // Calculate and print buy-side (bids) liquidity
    std::cout << "\nBuy-Side (Bids) Liquidity:" << std::endl;
    for (const auto& [exchange_name, order_book] : *m_order_books) 
    {
        double exchange_bid_volume = 0.0;
        const auto& bids = order_book->get_bids();
        for (const auto& [price, volume] : bids) 
        {
            exchange_bid_volume += volume;
        }
        total_buy_liquidity += exchange_bid_volume;
        total_buy_levels += bids.size();
        
        std::cout << std::left << std::setw(10) << exchange_name 
                  << ": " << std::fixed << std::setprecision(5) << exchange_bid_volume
                  << " units across " << bids.size() << " price levels" << std::endl;
    }

    // Calculate and print sell-side (asks) liquidity
    std::cout << "\nSell-Side (Asks) Liquidity:" << std::endl;
    for (const auto& [exchange_name, order_book] : *m_order_books) 
    {
        double exchange_ask_volume = 0.0;
        const auto& asks = order_book->get_asks();
        for (const auto& [price, volume] : asks) 
        {
            exchange_ask_volume += volume;
        }
        total_sell_liquidity += exchange_ask_volume;
        total_sell_levels += asks.size();
        
        std::cout << std::left << std::setw(10) << exchange_name 
                  << ": " << std::fixed << std::setprecision(8) << exchange_ask_volume
                  << " units across " << asks.size() << " price levels" << std::endl;
    }

    // Print totals
    std::cout << "\nTotal Liquidity:" << std::endl;
    std::cout << "Buy-Side : " << std::fixed << std::setprecision(8) << total_buy_liquidity
              << " units across " << total_buy_levels << " price levels" << std::endl;
    std::cout << "Sell-Side: " << std::fixed << std::setprecision(8) << total_sell_liquidity
              << " units across " << total_sell_levels << " price levels" << std::endl;

    // Print best bids and asks
    std::cout << "\nBest Available Prices:" << std::endl;
    for (const auto& [exchange_name, order_book] : *m_order_books) 
    {
        auto best_bid = order_book->get_best_bid();
        auto best_ask = order_book->get_best_ask();
        
        std::cout << std::left << std::setw(10) << exchange_name 
                  << ": Best Bid = " << std::setw(10) << best_bid.first 
                  << " (" << best_bid.second << " units)"
                  << " | Best Ask = " << std::setw(10) << best_ask.first 
                  << " (" << best_ask.second << " units)"
                  << std::endl;
    }
    std::cout << "=======================================\n" << std::endl;
}