#include "../include/orderbook.h"
#include "../include/smartorderrouter.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <memory>

// Function to read a CSV file and populate an OrderBook
void read_csv(const std::string& filename, OrderBook& order_book) 
{
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return;
    }

    std::string line;
    std::getline(file, line); // Skip the header row

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string price_str, volume_str, type;

        std::getline(ss, price_str, ',');
        std::getline(ss, volume_str, ',');
        std::getline(ss, type, ',');

        double price = std::stod(price_str);
        double volume = std::stod(volume_str);

        if (type == "Bid") {
            order_book.add_bid(price, volume);
        } else if (type == "Ask") {
            order_book.add_ask(price, volume);
        }
    }

    file.close();
}

int main() 
{
    // Create order books for each exchange with taker fees and minimum order sizes
    auto binance = std::make_shared<OrderBook>("Binance", 0.001, 0.001); // 0.1% taker fee, 0.001 min order size
    auto kucoin = std::make_shared<OrderBook>("KuCoin", 0.0005, 0.01);  // 0.05% taker fee, 0.01 min order size
    auto okx = std::make_shared<OrderBook>("OKX", 0.0002, 0.001);       // 0.02% taker fee, 0.001 min order size
    auto uniswap = std::make_shared<OrderBook>("Uniswap", 0.003, 0.1);  // 0.3% taker fee, 0.1 min order size

    // Read data from CSV files
    read_csv("../data/binance_order_book.csv", *binance);
    read_csv("../data/kucoin_order_book.csv", *kucoin);
    read_csv("../data/okx_order_book.csv", *okx);
    read_csv("../data/uniswap_order_book.csv", *uniswap);

    // Print order books
    /*binance->print_order_book();
    kucoin->print_order_book();
    okx->print_order_book();
    uniswap->print_order_book();
    */
    // Create a SmartOrderRouter with the order books
    std::unordered_map<std::string, std::shared_ptr<OrderBook>> order_books = {
        {"Binance", binance},
        {"KuCoin", kucoin},
        {"OKX", okx},
        {"Uniswap", uniswap}
    };
    SmartOrderRouter router(order_books);

    // Define order parameters
    double order_size = 1.0; // Total quantity to buy/sell
    bool is_buy = true;      // Set to false for a sell order

    // Distribute the order across exchanges
    auto execution_plan = router.distribute_order(order_size, is_buy);

    // Print execution plan
    std::cout << "Execution Plan:" << std::endl;
    for (const auto& fill : execution_plan) {
        std::cout << "Exchange: " << fill.first
                  << ", Price: " << fill.second.first
                  << ", Quantity: " << fill.second.second << std::endl;
    }

    return 0;
}