#include "../include/orderbook.h"
#include <limits>

// Constructor
OrderBook::OrderBook(const std::string& exchange_name, double taker_fee, double min_order_size)
    : exchange_name(exchange_name), taker_fee(taker_fee), min_order_size(min_order_size) {}

void OrderBook::add_bid(Price price, Volume volume) {
    bids[price] += volume; // Aggregate volumes at the same price
}

void OrderBook::add_ask(Price price, Volume volume) {
    asks[price] += volume; // Aggregate volumes at the same price
}

void OrderBook::remove_top_bid() {
    if (!bids.empty()) {
        bids.erase(--bids.end());
    } else 
    {
        throw std::runtime_error("No bids available to remove.");
    }
}

void OrderBook::remove_top_ask() {
    if (!asks.empty()) {
        asks.erase(asks.begin());
    }
    else 
    {
        throw std::runtime_error("No asks available to remove.");
    }
}

std::pair<Price, Volume> OrderBook::get_best_bid() const {
    if (bids.empty()) {
        return {0.0, 0.0}; // No bids available
    }
    return *bids.rbegin(); // Last element in the map
}

std::pair<Price, Volume> OrderBook::get_best_ask() const {
    if (asks.empty()) {
        return {0.0, 0.0}; // No asks available
    }
    return *asks.begin(); // First element in the map
}

double OrderBook::get_taker_fee() const {
    return taker_fee;
}

double OrderBook::get_min_order_size() const {
    return min_order_size;
}

const std::map<Price, Volume>& OrderBook::get_bids() const {
    return bids;
}

const std::map<Price, Volume>& OrderBook::get_asks() const {
    return asks;
}

std::string OrderBook::get_exchange_name() const {
    return exchange_name;
}

// Function to print the order book
void OrderBook::print_order_book() const {
    std::cout << "Order Book for " << exchange_name << ":" << std::endl;
    std::cout << "Taker Fee: " << taker_fee * 100 << "%" << std::endl;
    std::cout << "Minimum Order Size: " << min_order_size << std::endl;

    std::cout << "Bids:" << std::endl;
    for (auto it = bids.rbegin(); it != bids.rend(); ++it) {
        std::cout << "Price: " << it->first << ", Volume: " << it->second << std::endl;
    }

    std::cout << "Asks:" << std::endl;
    for (const auto& entry : asks) {
        std::cout << "Price: " << entry.first << ", Volume: " << entry.second << std::endl;
    }

    std::cout << std::endl;
}