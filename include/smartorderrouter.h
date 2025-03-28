#ifndef SMARTORDERROUTER_H
#define SMARTORDERROUTER_H

#ifdef DEBUG_MODE
    #define DEBUG_LOG(x) std::cout << x << std::endl
#else
    #define DEBUG_LOG(x)
#endif

#include "executionplan.h"
#include "orderbook.h"
#include <queue>
#include <vector>
#include <functional>
#include <memory>

enum class RoutingAlgorithm 
{
    PURE_GREEDY,
    HYBRID   
};

struct DPFill 
{
    ExchangeName exchange_name;
    Price price;
    Volume volume;
};

struct BestOrder {
    ExchangeName exchange_name;
    Price effective_price;
    Volume volume;
    Price original_price;
    double fee;
    
    struct BuyComparator 
    {
        bool operator()(const BestOrder& a, const BestOrder& b) const 
        {
            return a.effective_price > b.effective_price;
        }
    };
    
    struct SellComparator 
    {
        bool operator()(const BestOrder& a, const BestOrder& b) const 
        {
            return a.effective_price < b.effective_price;
        }
    };
};

class SmartOrderRouter 
{
private:
    std::unique_ptr<std::unordered_map<ExchangeName, std::shared_ptr<OrderBook>>> m_order_books;
    
    using Comparator = std::function<bool(const BestOrder&, const BestOrder&)>;
    
    Volume get_largest_min_lot_size(const std::priority_queue<BestOrder, std::vector<BestOrder>, Comparator>& best_orders) const;
    std::vector<FillOrder> distribute_order_optimized(Volume remaining_size, OrderSide side, const std::priority_queue<BestOrder, std::vector<BestOrder>, Comparator>& best_orders) const;

public:
    SmartOrderRouter(std::unordered_map<ExchangeName, std::shared_ptr<OrderBook>> order_books);
    SmartOrderRouter(SmartOrderRouter&& other) noexcept = default;
    SmartOrderRouter(const SmartOrderRouter&) = delete;
    SmartOrderRouter& operator=(const SmartOrderRouter&) = delete;
    ExecutionPlan distribute_order(Volume order_size, OrderSide m_side, RoutingAlgorithm algorithm = RoutingAlgorithm::HYBRID) const;

    void print_remaining_liquidity() const;
};

#endif // SMARTORDERROUTER_H