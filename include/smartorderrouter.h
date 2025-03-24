#ifndef SMARTORDERROUTER_H
#define SMARTORDERROUTER_H

#include "executionplan.h"
#include "orderbook.h"
#include <queue>
#include <vector>
#include <functional>
#include <memory>

struct BestOrder {
    ExchangeName exchange_name;
    Price effective_price;
    Volume volume;
    Price original_price;
    double fee;
    
    struct BuyComparator {
        bool operator()(const BestOrder& a, const BestOrder& b) const {
            return a.effective_price > b.effective_price;
        }
    };
    
    struct SellComparator {
        bool operator()(const BestOrder& a, const BestOrder& b) const {
            return a.effective_price < b.effective_price;
        }
    };
};

class SmartOrderRouter {
private:
    std::unique_ptr<std::unordered_map<ExchangeName, std::shared_ptr<OrderBook>>> order_books;
    
    using Comparator = std::function<bool(const BestOrder&, const BestOrder&)>;
    
    double get_largest_min_lot_size(
        const std::priority_queue<BestOrder, std::vector<BestOrder>, Comparator>& best_orders) const;

public:
    SmartOrderRouter(std::unordered_map<ExchangeName, std::shared_ptr<OrderBook>> order_books);
    SmartOrderRouter(SmartOrderRouter&& other) noexcept = default;
    SmartOrderRouter(const SmartOrderRouter&) = delete;
    SmartOrderRouter& operator=(const SmartOrderRouter&) = delete;
    
    ExecutionPlan distribute_order(Volume order_size, bool is_buy) const;
};

#endif // SMARTORDERROUTER_H