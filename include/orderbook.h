#ifndef ORDERBOOK_H
#define ORDERBOOK_H

#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip> // For std::fixed and std::setprecision
#include <cmath>
#include <memory> // For std::shared_ptr

// Aliases for price and volume
using Price = double;
using Volume = double;

class OrderBook {
private:
    std::map<Price, Volume> bids; // Key: Price, Value: Total volume at that price
    std::map<Price, Volume> asks; // Key: Price, Value: Total volume at that price
    std::string exchange_name;   // Name of the exchange
    double taker_fee;            // Taker fee for the exchange
    double min_order_size;       // Minimum order size for the exchange

public:
    OrderBook(const std::string& exchange_name, double taker_fee, double min_order_size);

    void add_bid(Price price, Volume volume);
    void add_ask(Price price, Volume volume);
    void remove_top_bid(); // Remove the top bid order
    void remove_top_ask(); // Remove the top ask order
    std::pair<Price, Volume> get_best_bid() const;
    std::pair<Price, Volume> get_best_ask() const;
    double get_taker_fee() const;
    double get_min_order_size() const;
    const std::map<Price, Volume>& get_bids() const;
    const std::map<Price, Volume>& get_asks() const;
    std::string get_exchange_name() const;
    void print_order_book() const;
};

#endif // ORDERBOOK_H