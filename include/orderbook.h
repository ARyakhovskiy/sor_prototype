#ifndef ORDERBOOK_H
#define ORDERBOOK_H

#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <memory>

// Aliases for price and volume
using Price = double;
using Volume = double;
using ExchangeName = std::string;

enum class OrderSide 
{
    BUY,
    SELL
};


class OrderBook 
{
private:
    std::map<Price, Volume> m_bids; // Key: Price, Value: Total volume at that price
    std::map<Price, Volume> m_asks; // Key: Price, Value: Total volume at that price
    ExchangeName m_exchange_name;   // Name of the exchange
    double m_taker_fee;            // Taker fee for the exchange
    Volume min_order_size;       // Minimum order size for the exchange

public:
    OrderBook(const std::string& exchange_name, double taker_fee, double min_order_size);

    void add_bid(Price price, Volume volume);
    void add_ask(Price price, Volume volume);
    void reduce_bid_volume(Price price, Volume reduction);
    void reduce_ask_volume(Price price, Volume reduction);
    Volume get_bid_volume(Price price) const;
    Volume get_ask_volume(Price price) const;
    void remove_top_bid();
    void remove_top_ask();
    std::pair<Price, Volume> get_best_bid() const;
    std::pair<Price, Volume> get_best_ask() const;
    double get_taker_fee() const;
    Volume get_min_order_size() const;
    const std::map<Price, Volume>& get_bids() const;
    const std::map<Price, Volume>& get_asks() const;
    const ExchangeName get_exchange_name() const;
    void print_order_book() const;
};

#endif // ORDERBOOK_H