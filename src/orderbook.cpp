#include "orderbook.h"
#include <limits>

// Constructor
OrderBook::OrderBook(const std::string& exchange_name, double taker_fee, double min_order_size)
    : m_exchange_name(exchange_name), m_taker_fee(taker_fee), min_order_size(min_order_size) {}

void OrderBook::add_bid(Price price, Volume volume) 
{
    m_bids[price] += volume; // Aggregate volumes at the same price
}

void OrderBook::add_ask(Price price, Volume volume) 
{
    m_asks[price] += volume; // Aggregate volumes at the same price
}

void OrderBook::remove_top_bid() 
{
    if (!m_bids.empty()) 
    {
        m_bids.erase(--m_bids.end());
    } else 
    {
        throw std::runtime_error("No bids available to remove.");
    }
}

void OrderBook::remove_top_ask() {
    if (!m_asks.empty()) 
    {
        m_asks.erase(m_asks.begin());
    }
    else 
    {
        throw std::runtime_error("No asks available to remove.");
    }
}

std::pair<Price, Volume> OrderBook::get_best_bid() const 
{
    if (m_bids.empty()) 
    {
        return {0.0, 0.0}; // No bids available
    }
    return *m_bids.rbegin();
}

std::pair<Price, Volume> OrderBook::get_best_ask() const {
    if (m_asks.empty()) 
    {
        return {0.0, 0.0}; // No asks available
    }
    return *m_asks.begin(); // First element in the map
}

double OrderBook::get_taker_fee() const {
    return m_taker_fee;
}

double OrderBook::get_min_order_size() const {
    return min_order_size;
}

const std::map<Price, Volume>& OrderBook::get_bids() const {
    return m_bids;
}

const std::map<Price, Volume>& OrderBook::get_asks() const {
    return m_asks;
}

const ExchangeName OrderBook::get_exchange_name() const 
{
    return m_exchange_name;
}

void OrderBook::print_order_book() const 
{
    std::cout << "Order Book for " << m_exchange_name << ":" << std::endl;
    std::cout << "Taker Fee: " << m_taker_fee * 100 << "%" << std::endl;
    std::cout << "Minimum Order Size: " << min_order_size << std::endl;

    std::cout << "Bids:" << std::endl;
    for (auto it = m_bids.rbegin(); it != m_bids.rend(); ++it) {
        std::cout << "Price: " << it->first << ", Volume: " << it->second << std::endl;
    }

    std::cout << "Asks:" << std::endl;
    for (const auto& entry : m_asks) {
        std::cout << "Price: " << entry.first << ", Volume: " << entry.second << std::endl;
    }

    std::cout << std::endl;
}