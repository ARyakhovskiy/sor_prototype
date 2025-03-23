// greedy_order_distribution.h
#ifndef GREEDY_ORDER_DISTRIBUTION_H
#define GREEDY_ORDER_DISTRIBUTION_H

#include "orderdistributionstrategy.h"

struct BestOrder {
    std::string exchange_name;
    Price effective_price; // Price after fees
    Volume volume;
    Price original_price;  // Original price before fees
    double fee;             // Taker fee for the exchange

    // Comparator for buy orders (lowest effective price first)
    struct BuyComparator 
    {
        bool operator()(const BestOrder& a, const BestOrder& b) const {
            return a.effective_price > b.effective_price; // Min-heap for buy orders
        }
    };

    // Comparator for sell orders (highest effective price first)
    struct SellComparator 
    {
        bool operator()(const BestOrder& a, const BestOrder& b) const {
            return a.effective_price < b.effective_price; // Max-heap for sell orders
        }
    };
};

class GreedyOrderDistribution : public OrderDistributionStrategy 
{
public:
    std::vector<std::pair<std::string, std::pair<Price, Volume>>> distribute_order(const std::unordered_map<std::string, std::shared_ptr<OrderBook>>& order_books,
                                                                                    double order_size,
                                                                                    bool is_buy) const override;
};

#endif // GREEDY_ORDER_DISTRIBUTION_H