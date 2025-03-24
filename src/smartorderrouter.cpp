#include "smartorderrouter.h"
#include <unordered_set>
#include <algorithm>
#include <iomanip>

constexpr double EPSILON = 1e-6;

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

    using Comparator = std::function<bool(const BestOrder&, const BestOrder&)>;
    Comparator comparator;
    if (side == OrderSide::BUY) 
    {
        comparator = BestOrder::BuyComparator();
    } 
    else 
    {
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

            std::cout << "\nAdded order to queue: Exchange = " << exchange_name
                      << ", Effective Price = " << effective_price(price, side, fee)
                      << ", Volume = " << volume
                      << ", MinLotSize = " << order_books_ptr->at(exchange_name)->get_min_order_size()
                      << ", Original Price = " << price
                      << ", Fee = " << fee << std::endl;
        }
    }

    Volume largest_min_lot_size = get_largest_min_lot_size(best_orders);
    std::cout << " \nlargest_min_lot_size "  << largest_min_lot_size << std::endl;

    while (remaining_size >= absolute_min_lot_size && !best_orders.empty()) {
        std::cout << "\nCurrent state of best_orders queue:" << std::endl;
        auto temp_queue = best_orders;
        while (!temp_queue.empty()) {
            const auto& order = temp_queue.top();
            std::cout << "Exchange = " << order.exchange_name
                      << ", Effective Price = " << order.effective_price
                      << ", Volume = " << order.volume
                      << ", MinLotSize = " << m_order_books->at(order.exchange_name)->get_min_order_size()
                      << ", Original Price = " << order.original_price
                      << ", Fee = " << order.fee << std::endl;
            temp_queue.pop();
        }

        auto best_order = best_orders.top();

        std::cout << "\nProcessing order: Exchange = " << best_order.exchange_name
                  << ", Effective Price = " << best_order.effective_price
                  << ", Volume = " << best_order.volume
                  << ", MinLotSize = " << m_order_books->at(best_order.exchange_name)->get_min_order_size()
                  << ", Original Price = " << best_order.original_price
                  << ", Fee = " << best_order.fee << std::endl;


        Volume fill_quantity = std::min(best_order.volume, remaining_size);
        Volume min_order_size = m_order_books->at(best_order.exchange_name)->get_min_order_size();


        fill_quantity = std::floor((fill_quantity / min_order_size) + EPSILON) * min_order_size;

        if (fill_quantity > 0) 
        {
            // Check if we should switch to optimized approach (if we're close to min_order_sizes)
            if (algorithm == RoutingAlgorithm::HYBRID && 
                remaining_size - fill_quantity > EPSILON &&
                remaining_size - fill_quantity < largest_min_lot_size) 
            {               
                std::vector<FillOrder> optimized_fills = distribute_order_optimized(remaining_size, side, best_orders);
                for (const FillOrder& fill : optimized_fills) 
                {
                    execution_plan.add_fill(fill);
                    remaining_size -= fill.volume;
                }
                break;
            }

            execution_plan.add_fill(FillOrder(best_order.exchange_name, best_order.original_price, fill_quantity));

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
        if (side == OrderSide::BUY) {
            order_book->remove_top_ask();
        } else {
            order_book->remove_top_bid();
        }

        best_orders.pop();

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

    return execution_plan;
}

std::vector<FillOrder> SmartOrderRouter::distribute_order_optimized(Volume remaining_size,
                                                                    OrderSide side,
                                                                    const std::priority_queue<BestOrder, std::vector<BestOrder>, Comparator>& best_orders
                                                                ) const 
{
    // Generate candidate lots (stop adding from an exchange if cumulative volume >= remaining_size)
    std::vector<FillOrder> available_lots;
    for (const auto& [exchange_name, order_book] : *m_order_books) {
        const auto& order_side = (side == OrderSide::BUY) ? order_book->get_asks() : order_book->get_bids();
        Volume min_size = order_book->get_min_order_size();
        Volume cumulative_volume = 0.0;

        for (const auto& [price, volume] : order_side) {
            Volume remaining_volume_at_level = volume;
            while (remaining_volume_at_level >= min_size && cumulative_volume < remaining_size + EPSILON) {
                available_lots.emplace_back(exchange_name, price, min_size);
                cumulative_volume += min_size;
                remaining_volume_at_level -= min_size;
            }
            if (cumulative_volume >= remaining_size) break; // Stop if exchange has enough volume
        }
    }

    // Sort by effective price (best first)
    std::sort(available_lots.begin(), available_lots.end(),
        [this, side](const FillOrder& a, const FillOrder& b) 
        {
            double fee_a = m_order_books->at(a.exchange_name)->get_taker_fee();
            double fee_b = m_order_books->at(b.exchange_name)->get_taker_fee();
            Price eff_a = (side == OrderSide::BUY) ? a.price * (1 + fee_a) : a.price * (1 - fee_a);
            Price eff_b = (side == OrderSide::BUY) ? b.price * (1 + fee_b) : b.price * (1 - fee_b);
            return (side == OrderSide::BUY) ? (eff_a < eff_b) : (eff_a > eff_b);
        });


    using MemoKey = std::pair<Volume, size_t>; // (remaining_volume, current_index)
    std::map<MemoKey, std::pair<Price, std::vector<FillOrder>>> memo;

    std::function<std::pair<Price, std::vector<FillOrder>>(Volume, size_t)> solve = [&](Volume remaining, size_t index) -> std::pair<Price, std::vector<FillOrder>> 
    {
        MemoKey key = {remaining, index};
        
        if (memo.count(key)) return memo[key];

        if (remaining <= EPSILON) return {0.0, {}};

        if (index >= available_lots.size()) 
        {
            return 
            {
                (side == OrderSide::BUY) ? std::numeric_limits<Price>::max() : std::numeric_limits<Price>::min(),
                {}
            };
        }

        const FillOrder& lot = available_lots[index];
        double fee = m_order_books->at(lot.exchange_name)->get_taker_fee();
        Price lot_cost = lot.volume * lot.price * (1 + ((side == OrderSide::BUY) ? fee : -fee));

        // Option 1: Take this lot (if possible)
        auto take_solution = (lot.volume <= remaining + EPSILON)
            ? solve(remaining - lot.volume, index + 1)
            : std::make_pair(
                (side == OrderSide::BUY) ? std::numeric_limits<Price>::max() 
                                        : std::numeric_limits<Price>::min(),
                std::vector<FillOrder>{}
                );
        take_solution.first += lot_cost;
        take_solution.second.push_back(lot);

        // Option 2: Skip this lot
        auto skip_solution = solve(remaining, index + 1);

        bool take_is_better = (side == OrderSide::BUY) 
            ? (take_solution.first < skip_solution.first)
            : (take_solution.first > skip_solution.first);

        memo[key] = take_is_better ? take_solution : skip_solution;
        return memo[key];
    };

    // Solve for exact fill
    auto [total_cost, solution] = solve(remaining_size, 0);

    // If no exact solution, find the maximum undershoot
    if (solution.empty() || 
        (side == OrderSide::BUY && total_cost == std::numeric_limits<Price>::max()) ||
        (side == OrderSide::SELL && total_cost == std::numeric_limits<Price>::min())) {
        
        std::cout << "No exact solution found. Searching for maximum undershoot...\n";
        Volume best_undershoot = 0.0;
        Price best_cost = (side == OrderSide::BUY) 
            ? std::numeric_limits<Price>::max() 
            : std::numeric_limits<Price>::min();
        solution.clear();

        std::function<void(size_t, Volume, Price, std::vector<FillOrder>&)> backtrack =
            [&](size_t start_idx, Volume current_volume, Price current_cost, std::vector<FillOrder>& current) {
                if (current_volume > remaining_size + EPSILON) return;

                // Update best solution if current undershoot is better
                if (current_volume > best_undershoot + EPSILON ||
                    (std::abs(current_volume - best_undershoot) <= EPSILON &&
                     ((side == OrderSide::BUY && current_cost < best_cost) ||
                      (side == OrderSide::SELL && current_cost > best_cost)))) {
                    best_undershoot = current_volume;
                    best_cost = current_cost;
                    solution = current;
                }

                for (size_t i = start_idx; i < available_lots.size(); ++i) {
                    const FillOrder& lot = available_lots[i];
                    double fee = m_order_books->at(lot.exchange_name)->get_taker_fee();
                    Price cost = lot.volume * lot.price * (1 + ((side == OrderSide::BUY) ? fee : -fee));

                    if (current_volume + lot.volume <= remaining_size + EPSILON) {
                        current.push_back(lot);
                        backtrack(i + 1, current_volume + lot.volume, current_cost + cost, current);
                        current.pop_back();
                    }
                }
            };

        std::vector<FillOrder> current;
        backtrack(0, 0.0, 0.0, current);
        total_cost = best_cost;
    }


    // Print results
    std::cout << "\n=== Optimal Solution ===\n";
    Volume total_volume = 0.0;
    Price total_fees = 0.0;
    for (const auto& fill : solution) {
        double fee = m_order_books->at(fill.exchange_name)->get_taker_fee();
        Price eff_price = (side == OrderSide::BUY) ? fill.price * (1 + fee) : fill.price * (1 - fee);
        Price fill_cost = fill.volume * fill.price;
        Price fill_fee = fill_cost * fee;

        std::cout << "Exchange: " << std::setw(8) << fill.exchange_name
                  << " | Price: " << std::setw(10) << fill.price
                  << " | Volume: " << std::setw(8) << fill.volume
                  << " | Eff. Price: " << std::setw(12) << eff_price
                  << " | Fees: " << std::setw(8) << fill_fee << "\n";

        total_volume += fill.volume;
        total_fees += fill_fee;
    }

    std::cout << "\nSummary:\n";
    std::cout << "Total Volume: " << total_volume << "\n";
    std::cout << "Total Cost: " << total_cost << "\n";
    std::cout << "Total Fees: " << total_fees << "\n";
    std::cout << "Effective Price: " << total_cost / std::max(total_volume, EPSILON) << "\n";
    std::cout << "====================================\n";

    return solution;
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