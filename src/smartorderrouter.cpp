#include "smartorderrouter.h"
#include <unordered_set>
#include <algorithm>
#include <iomanip>

constexpr double EPSILON = 1e-12;

SmartOrderRouter::SmartOrderRouter(std::unordered_map<ExchangeName, std::shared_ptr<OrderBook>> order_books)
    : m_order_books(std::make_unique<std::unordered_map<ExchangeName, std::shared_ptr<OrderBook>>>(std::move(order_books))) {}

Price effective_price(Price original_price, OrderSide side, double fee) {
    return (side == OrderSide::BUY) ? original_price * (1 + fee) : original_price * (1 - fee);
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

ExecutionPlan SmartOrderRouter::distribute_order(Volume order_size, OrderSide side, RoutingAlgorithm algorithm) const
{
    // Create aliasing shared_ptr for order books
    auto order_books_ptr = std::shared_ptr<const std::unordered_map<ExchangeName, std::shared_ptr<OrderBook>>>(
        m_order_books.get(),
        [](auto*) {} // No-op deleter
    );
    
    // Create empty execution plan at start
    ExecutionPlan execution_plan({}, order_books_ptr, side, order_size);

    Volume remaining_size = order_size;
    Volume absolute_min_lot_size = order_size;

    // Initialize priority queue with original comparator logic
    using Comparator = std::function<bool(const BestOrder&, const BestOrder&)>;
    Comparator comparator;
    if (side == OrderSide::BUY) {
        comparator = BestOrder::BuyComparator();
    } else {
        comparator = BestOrder::SellComparator();
    }
    std::priority_queue<BestOrder, std::vector<BestOrder>, Comparator> best_orders(comparator);

    std::cout << "Initial Order: Size = " << order_size << ", Type = " << ((side == OrderSide::BUY) ? "Buy" : "Sell") << std::endl;

    // Initialize priority queue with best orders from each exchange
    for (const auto& [exchange_name, order_book] : *m_order_books) {
        const auto& order_side = (side == OrderSide::BUY) ? order_book->get_asks() : order_book->get_bids();

        if (!order_side.empty()) {
            auto best_order = (side == OrderSide::BUY) ? order_side.begin() : --order_side.end();
            Price price = best_order->first;
            Volume volume = best_order->second;
            double fee = order_book->get_taker_fee();
            
            absolute_min_lot_size = std::min(absolute_min_lot_size, order_book->get_min_order_size());

            best_orders.push({
                exchange_name,
                effective_price(price, side, fee),
                volume,
                price,
                fee
            });

            std::cout << "Added order to queue: Exchange = " << exchange_name
                      << ", Effective Price = " << effective_price(price, side, fee)
                      << ", Volume = " << volume
                      << ", Original Price = " << price
                      << ", Fee = " << fee << std::endl;
        }
    }

    Volume largest_min_lot_size = get_largest_min_lot_size(best_orders);

    while (remaining_size >= absolute_min_lot_size && !best_orders.empty()) {
        std::cout << "\nCurrent state of best_orders queue:" << std::endl;
        auto temp_queue = best_orders;
        while (!temp_queue.empty()) {
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

        if (fill_quantity > 0) {
            execution_plan.add_fill(FillOrder(best_order.exchange_name, best_order.original_price, fill_quantity));

            std::cout << "Added to execution plan: Exchange = " << best_order.exchange_name
                      << ", Price = " << best_order.original_price
                      << ", Quantity = " << fill_quantity << std::endl;

            Price fees = fill_quantity * best_order.original_price * best_order.fee;
            std::cout << "Fees for this fill: " << fees << std::endl;
            remaining_size -= fill_quantity;
            std::cout << "Remaining size to fill: " << remaining_size << std::endl;

            // Check if we should switch to DP approach
            if (algorithm == RoutingAlgorithm::HYBRID && remaining_size < largest_min_lot_size) {
                std::cout << "Switching to dynamic programming approach for remaining " 
                          << remaining_size << " units\n";
                
                auto dp_fills = solve_knapsack_problem(remaining_size, side, best_orders);
                for (const auto& fill : dp_fills) {
                    execution_plan.add_fill(fill);
                    remaining_size -= fill.volume;
                }
                break;
            }
        } else {
            std::cout << "Skipping order from " << best_order.exchange_name
                      << " because fill_quantity <= 0." << std::endl;
            std::cout << "Remaining size to fill: " << remaining_size << std::endl;
        }

        // Update order book
        auto& order_book = m_order_books->at(best_order.exchange_name);
        if (side == OrderSide::BUY) {
            order_book->remove_top_ask();
        } else {
            order_book->remove_top_bid();
        }

        // Add next order from same exchange if available
        const auto& order_side = (side == OrderSide::BUY) ? order_book->get_asks() : order_book->get_bids();
        if (order_side.empty()) {
            largest_min_lot_size = get_largest_min_lot_size(best_orders);
        }

        if (!order_side.empty() && order_book->get_min_order_size() <= remaining_size) {
            auto next_order = (side == OrderSide::BUY) ? order_side.begin() : --order_side.end();
            Price price = next_order->first;
            Volume volume = next_order->second;
            double fee = order_book->get_taker_fee();

            best_orders.push({
                best_order.exchange_name,
                effective_price(price, side, fee),
                volume,
                price,
                fee
            });

            std::cout << "Added next order to queue: Exchange = " << best_order.exchange_name
                      << ", Effective Price = " << effective_price(price, side, fee)
                      << ", Volume = " << volume
                      << ", Original Price = " << price
                      << ", Fee = " << fee << std::endl;
        }        
    }

    std::cout << "Total Fees Paid: " << std::fixed << std::setprecision(2) << execution_plan.get_total_fees() << std::endl;
    print_remaining_liquidity();

    return execution_plan;
}

std::vector<FillOrder> SmartOrderRouter::solve_knapsack_problem(
    Volume remaining_size, 
    OrderSide side,
    const std::priority_queue<BestOrder, std::vector<BestOrder>, Comparator>& best_orders
) const
{
    std::cout << "\n=== Starting Dynamic Programming Solution ===" << std::endl;
    std::cout << "Remaining size to fill: " << remaining_size << std::endl;
    
    std::vector<FillOrder> dp_fills;
    Volume absolute_min_lot_size = get_largest_min_lot_size(best_orders);
    std::cout << "Largest minimum lot size: " << absolute_min_lot_size << std::endl;

    // Collect all available lots
    std::vector<FillOrder> available_lots;
    for (const auto& [exchange_name, order_book] : *m_order_books) {
        const auto& order_side = (side == OrderSide::BUY) ? order_book->get_asks() : order_book->get_bids();
        Volume min_size = order_book->get_min_order_size();
        
        for (const auto& [price, volume] : order_side) {
            Volume max_possible = std::min(volume, remaining_size);
            size_t num_lots = static_cast<size_t>(max_possible / min_size);
            
            if (num_lots > 0) {
                available_lots.emplace_back(exchange_name, price, min_size);
            }
        }
    }

    std::cout << "\nAvailable lots before sorting (" << available_lots.size() << "):" << std::endl;
    for (const auto& [exchange_name, price, volume] : available_lots) {
        double fee = m_order_books->at(exchange_name)->get_taker_fee();
        Price eff_price = effective_price(price, side, fee);
        std::cout << "Exchange: " << std::setw(10) << exchange_name 
                  << " | Price: " << std::setw(10) << price
                  << " | Eff Price: " << std::setw(12) << eff_price
                  << " | Lot Size: " << volume << std::endl;
    }

    // Sort lots by effective price
    auto order_books_ptr = m_order_books.get();
    std::sort(available_lots.begin(), available_lots.end(),
        [side, order_books_ptr](const FillOrder& a, const FillOrder& b) {
            double fee_a = order_books_ptr->at(a.exchange_name)->get_taker_fee();
            double fee_b = order_books_ptr->at(b.exchange_name)->get_taker_fee();
            Price eff_a = effective_price(a.price, side, fee_a);
            Price eff_b = effective_price(b.price, side, fee_b);
            return (side == OrderSide::BUY) ? (eff_a < eff_b) : (eff_a > eff_b);
        });

    std::cout << "\nAvailable lots after sorting:" << std::endl;
    for (const auto& [exchange_name, price, volume] : available_lots) {
        double fee = m_order_books->at(exchange_name)->get_taker_fee();
        Price eff_price = effective_price(price, side, fee);
        std::cout << "Exchange: " << std::setw(10) << exchange_name 
                  << " | Price: " << std::setw(10) << price
                  << " | Eff Price: " << std::setw(12) << eff_price
                  << " | Lot Size: " << volume << std::endl;
    }

    // DP table initialization
    size_t max_lots = static_cast<size_t>(remaining_size / absolute_min_lot_size);
    std::vector<std::vector<Price>> dp_table(available_lots.size() + 1, 
                                           std::vector<Price>(max_lots + 1, 0));
    std::vector<std::vector<FillOrder>> solution_table(available_lots.size() + 1, 
                                                  std::vector<FillOrder>(max_lots + 1));

    std::cout << "\nRunning DP algorithm with " << available_lots.size() 
              << " items and max " << max_lots << " lots" << std::endl;

    // DP solution
    for (size_t i = 1; i <= available_lots.size(); ++i) {
        const auto& [exchange_name, price, volume] = available_lots[i-1];
        double fee = order_books_ptr->at(exchange_name)->get_taker_fee();
        Price effective_price_value = (side == OrderSide::BUY) ? 
            price * (1 + fee) : price * (1 - fee);
        
        for (size_t j = 1; j <= max_lots; ++j) {
            Volume current_volume = volume * j;
            if (current_volume > remaining_size) continue;
            
            // Find best previous solution for remaining volume
            Volume remaining_volume = remaining_size - current_volume;
            size_t best_prev = 0;
            Price best_value = 0;
            
            for (size_t k = 0; k < i; ++k) {
                if (dp_table[k][remaining_volume] > best_value) {
                    best_value = dp_table[k][remaining_volume];
                    best_prev = k;
                }
            }
            
            // Update DP table
            Price new_value = current_volume * effective_price_value + best_value;
            if (new_value > dp_table[i][remaining_size]) {
                dp_table[i][remaining_size] = new_value;
                solution_table[i][remaining_size] = available_lots[i-1];
                solution_table[i][remaining_size].volume = current_volume;
            }
        }
    }

    // Backtrack to find optimal solution
    Volume remaining = remaining_size;
    size_t i = available_lots.size();
    while (remaining > 0 && i > 0) {
        if (dp_table[i][remaining] != dp_table[i-1][remaining]) {
            const auto& fill = solution_table[i][remaining];
            dp_fills.emplace_back(fill);
            remaining -= fill.volume;
        }
        --i;
    }

    // Print DP solution
    std::cout << "\n=== DP Solution Found ===" << std::endl;
    std::cout << "Total fills: " << dp_fills.size() << std::endl;
    Price total_cost = 0;
    Price total_fees = 0;
    Volume total_volume = 0;
    
    for (const auto& [exchange_name, price, volume] : dp_fills) 
    {
        double fee = m_order_books->at(exchange_name)->get_taker_fee();
        Price eff_price = effective_price(price, side, fee);
        Price fill_cost = volume * price;
        Price fill_fee = fill_cost * fee;
        
        std::cout << "Exchange: " << std::setw(10) << exchange_name 
                  << " | Price: " << std::setw(10) << price
                  << " | Eff Price: " << std::setw(12) << eff_price
                  << " | Volume: " << std::setw(10) << volume
                  << " | Cost: " << std::setw(12) << fill_cost
                  << " | Fees: " << std::setw(10) << fill_fee << std::endl;
        
        total_volume += volume;
        total_cost += fill_cost;
        total_fees += fill_fee;
    }

    Price total_effective_cost = (side == OrderSide::BUY) ? 
        (total_cost + total_fees) : (total_cost - total_fees);
    
    std::cout << "\nSummary:" << std::endl;
    std::cout << "Total Volume: " << total_volume << std::endl;
    std::cout << "Total Cost: " << total_cost << std::endl;
    std::cout << "Total Fees: " << total_fees << std::endl;
    std::cout << "Total Effective Cost: " << total_effective_cost << std::endl;
    std::cout << "====================================\n" << std::endl;

    return dp_fills;
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