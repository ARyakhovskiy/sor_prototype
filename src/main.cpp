#include "../include/orderbook.h"
#include "../include/smartorderrouter.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <memory>

void read_csv(const std::string& filename, OrderBook& order_book) 
{
    std::ifstream file(filename);

    if (!file.is_open()) 
    {
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

        Price price = std::stod(price_str);
        Volume volume = std::stod(volume_str);

        if (type == "Bid") 
        {
            order_book.add_bid(price, volume);
        }
        else if (type == "Ask")
        {
            order_book.add_ask(price, volume);
        }
    }

    file.close();
}

int main() 
{
    auto binance = std::make_shared<OrderBook>("Binance", 0.001, 0.001); // 0.1% taker fee, 0.001 min order size
    auto kucoin = std::make_shared<OrderBook>("KuCoin", 0.0005, 0.01);  // 0.05% taker fee, 0.01 min order size
    auto okx = std::make_shared<OrderBook>("OKX", 0.0002, 0.001);       // 0.02% taker fee, 0.001 min order size
    auto uniswap = std::make_shared<OrderBook>("Uniswap", 0.003, 0.1);  // 0.3% taker fee, 0.1 min order size

    read_csv("../data/snap2/binance_order_book.csv", *binance);
    read_csv("../data/snap2/kucoin_order_book.csv", *kucoin);
    read_csv("../data/snap2/okx_order_book.csv", *okx);

    // Print order books
    /*binance->print_order_book();
    kucoin->print_order_book();
    okx->print_order_book();
    uniswap->print_order_book();
    */

    std::unordered_map<std::string, std::shared_ptr<OrderBook>> order_books = 
    {
        {"Binance", binance},
        {"KuCoin", kucoin},
        {"OKX", okx},
        {"Uniswap", uniswap}
    };
    
    SmartOrderRouter router(std::move(order_books));

    while (true) 
    {
        double order_size;
        std::cout << "Enter the number of units to buy (positive) or sell (negative). Enter 0 to exit: ";
        std::cin >> order_size;

        if (std::cin.fail()) {
            std::cerr << "Invalid input. Please enter a number." << std::endl;
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        if (order_size == 0) {
            std::cout << "Exiting the program." << std::endl;
            break;
        }

        bool is_buy = (order_size > 0);

        ExecutionPlan execution_plan = router.distribute_order(std::abs(order_size), is_buy);

        execution_plan.print();
    }

}