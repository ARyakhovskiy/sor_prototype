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
            // Check if we should switch to DP approach
            if (algorithm == RoutingAlgorithm::HYBRID && 
                remaining_size - fill_quantity > EPSILON &&
                remaining_size - fill_quantity < largest_min_lot_size) 
            {
                std::cout << "\nSwitching to dynamic programming approach for remaining " 
                          << remaining_size << " units\n";
                
                auto dp_fills = distribute_order_optimized(remaining_size, side, best_orders);
                for (const auto& fill : dp_fills) {
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

std::vector<FillOrder> SmartOrderRouter::distribute_order_optimized(
    Volume remaining_size,
    OrderSide side,
    const std::priority_queue<BestOrder, std::vector<BestOrder>, Comparator>& best_orders
) const {
    std::cout << "Remaining size to fill: " << remaining_size << std::endl;

    // Generate all possible lots (respecting min order sizes)
    std::vector<FillOrder> available_lots;
    for (const auto& [exchange_name, order_book] : *m_order_books) {
        const auto& order_side = (side == OrderSide::BUY) ? order_book->get_asks() : order_book->get_bids();
        Volume min_size = order_book->get_min_order_size();

        for (const auto& [price, volume] : order_side) {
            int max_lots = static_cast<int>(volume / min_size);
            for (int i = 0; i < max_lots; i++) {
                available_lots.emplace_back(exchange_name, price, min_size);
            }
        }
    }

    // Sort by effective price (best first)
    std::sort(available_lots.begin(), available_lots.end(),
        [this, side](const FillOrder& a, const FillOrder& b) {
            double fee_a = m_order_books->at(a.exchange_name)->get_taker_fee();
            double fee_b = m_order_books->at(b.exchange_name)->get_taker_fee();
            Price eff_a = (side == OrderSide::BUY) ? a.price * (1 + fee_a) : a.price * (1 - fee_a);
            Price eff_b = (side == OrderSide::BUY) ? b.price * (1 + fee_b) : b.price * (1 - fee_b);
            return (side == OrderSide::BUY) ? (eff_a < eff_b) : (eff_a > eff_b);
        });

    // Memoization table: stores best solution for (remaining_volume, current_index)
    using MemoKey = std::pair<Volume, size_t>;
    struct MemoValue {
        Price cost;
        std::vector<FillOrder> solution;
    };
    std::map<MemoKey, MemoValue> memo;

    // Recursive helper with memoization
    std::function<MemoValue(Volume, size_t)> solve = [&](Volume remaining, size_t index) -> MemoValue {
        MemoKey key = {remaining, index};
        
        // Check memo table
        if (memo.count(key)) {
            return memo[key];
        }

        // Base case: order filled
        if (remaining <= EPSILON) {
            return {0.0, {}};
        }

        // Base case: no more lots to consider
        if (index >= available_lots.size()) {
            return {
                (side == OrderSide::BUY) ? std::numeric_limits<Price>::max() 
                                         : std::numeric_limits<Price>::min(),
                {}
            };
        }

        const FillOrder& lot = available_lots[index];
        double fee = m_order_books->at(lot.exchange_name)->get_taker_fee();
        Price lot_cost = lot.volume * lot.price * (1 + ((side == OrderSide::BUY) ? fee : -fee));

        // Option 1: Take this lot (if possible)
        MemoValue take_result;
        if (lot.volume <= remaining + EPSILON) {
            take_result = solve(remaining - lot.volume, index + 1);
            take_result.cost += lot_cost;
            take_result.solution.push_back(lot);
        } else {
            take_result = {
                (side == OrderSide::BUY) ? std::numeric_limits<Price>::max() 
                                             : std::numeric_limits<Price>::min(),
                {}
            };
        }

        // Option 2: Skip this lot
        MemoValue skip_result = solve(remaining, index + 1);

        // Determine which option is better
        bool take_is_better;
        if (side == OrderSide::BUY) {
            take_is_better = take_result.cost < skip_result.cost;
        } else {
            take_is_better = take_result.cost > skip_result.cost;
        }

        // Store result in memo table
        memo[key] = take_is_better ? take_result : skip_result;
        return memo[key];
    };

    // Find the best solution
    MemoValue result = solve(remaining_size, 0);

    // If no exact solution, find minimal overshoot
    if (result.solution.empty() || 
        (side == OrderSide::BUY && result.cost == std::numeric_limits<Price>::max()) ||
        (side == OrderSide::SELL && result.cost == std::numeric_limits<Price>::min())) {
        
        std::cout << "No exact solution found, looking for minimal overshoot...\n";
        Volume best_overshoot = std::numeric_limits<Volume>::max();
        Price best_cost = (side == OrderSide::BUY) ? std::numeric_limits<Price>::max() 
                                                  : std::numeric_limits<Price>::min();
        std::vector<FillOrder> best_overshoot_solution;

        for (const auto& lot : available_lots) {
            if (lot.volume >= remaining_size) {
                double fee = m_order_books->at(lot.exchange_name)->get_taker_fee();
                Price cost = lot.volume * lot.price * (1 + ((side == OrderSide::BUY) ? fee : -fee));
                Volume overshoot = lot.volume - remaining_size;

                if (overshoot < best_overshoot || 
                    (overshoot == best_overshoot && 
                     ((side == OrderSide::BUY && cost < best_cost) ||
                      (side == OrderSide::SELL && cost > best_cost)))) {
                    best_overshoot = overshoot;
                    best_cost = cost;
                    best_overshoot_solution = {lot};
                }
            }
        }
        result.solution = best_overshoot_solution;
        result.cost = best_cost;
    }

    // Print results
    std::cout << "\n=== Optimal Solution Found ===\n";
    Volume total_volume = 0.0;
    Price total_cost = 0.0;
    Price total_fees = 0.0;

    for (const auto& fill : result.solution) {
        double fee = m_order_books->at(fill.exchange_name)->get_taker_fee();
        Price eff_price = (side == OrderSide::BUY) ? fill.price * (1 + fee) : fill.price * (1 - fee);
        Price fill_cost = fill.volume * fill.price;
        Price fill_fee = fill_cost * fee;

        std::cout << "Exchange: " << std::setw(10) << fill.exchange_name 
                  << " | Price: " << std::setw(10) << fill.price
                  << " | Eff Price: " << std::setw(12) << eff_price
                  << " | Volume: " << std::setw(10) << fill.volume
                  << " | Cost: " << std::setw(12) << fill_cost
                  << " | Fees: " << std::setw(10) << fill_fee << std::endl;

        total_volume += fill.volume;
        total_cost += fill_cost;
        total_fees += fill_fee;
    }

    Price total_effective_cost = (side == OrderSide::BUY) ? (total_cost + total_fees) : (total_cost - total_fees);
    std::cout << "\nSummary:\n";
    std::cout << "Total Volume: " << total_volume << "\n";
    std::cout << "Total Cost: " << total_cost << "\n";
    std::cout << "Total Fees: " << total_fees << "\n";
    std::cout << "Total Effective Cost: " << total_effective_cost << "\n";
    std::cout << "====================================\n";

    return result.solution;
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