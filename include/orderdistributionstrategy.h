// orderdistributionstrategy.h
#ifndef ORDER_DISTRIBUTION_STRATEGY_H
#define ORDER_DISTRIBUTION_STRATEGY_H

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include "orderbook.h"

class OrderDistributionStrategy 
{
public:
    virtual ~OrderDistributionStrategy() = default;

    virtual std::vector<std::pair<std::string, std::pair<Price, Volume>>> distribute_order(const std::unordered_map<std::string, std::shared_ptr<OrderBook>>& order_books,
                                                                            Volume order_size,
                                                                            bool is_buy) const = 0;
};

#endif // ORDER_DISTRIBUTION_STRATEGY_H