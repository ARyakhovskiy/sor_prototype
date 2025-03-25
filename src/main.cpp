#include "orderbook.h"
#include "smartorderrouter.h"
#include "utils.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <memory>
#include <filesystem>
#include <cctype>

namespace fs = std::filesystem;

int main() 
{
    auto binance = std::make_shared<OrderBook>("Binance", 0.001, 0.1);  // 0.1% fee, 0.1 min order size
    auto kucoin = std::make_shared<OrderBook>("KuCoin", 0.0005, 0.15);  // 0.05% fee, 0.15 min order size
    auto okx = std::make_shared<OrderBook>("OKX", 0.0002, 0.2);         // 0.02% fee, 0.2 min order size

    // Get the current executable's directory
    fs::path data_dir = fs::path(__FILE__).parent_path().parent_path() / "data/";
    
    // Load order books using relative paths
    read_csv((data_dir / "binance_order_book.csv").string(), *binance);
    read_csv((data_dir / "kucoin_order_book.csv").string(), *kucoin);
    read_csv((data_dir / "okx_order_book.csv").string(), *okx);

    std::unordered_map<std::string, std::shared_ptr<OrderBook>> order_books = 
    {
        {"Binance", binance},
        {"KuCoin", kucoin},
        {"OKX", okx}
    };
    
    SmartOrderRouter router(std::move(order_books));

    while (true) 
    {
        std::string input;
        std::cout << "Enter order size (positive=Buy, negative=Sell), 'lq' to show books, or 'exit': ";
        std::getline(std::cin, input);
        
        if (input == "exit") 
        {
            break;
        }
        else if (input == "lq") 
        {
            router.print_remaining_liquidity();
            continue;
        }
    
        try 
        {
            double order_size = std::stod(input);
            if (order_size == 0) continue;

            char algorithm_choice;
            do 
            {
                std::cout << "Choose algorithm - [G]reedy or [H]ybrid (G/H): ";
                std::cin >> algorithm_choice;
                algorithm_choice = static_cast<char>(toupper(algorithm_choice));
            } while (algorithm_choice != 'G' && algorithm_choice != 'H');

            RoutingAlgorithm algorithm = (algorithm_choice == 'G') 
                ? RoutingAlgorithm::PURE_GREEDY 
                : RoutingAlgorithm::HYBRID;
   
            OrderSide side = (order_size > 0) ? OrderSide::BUY : OrderSide::SELL;
            ExecutionPlan execution_plan = router.distribute_order(std::abs(order_size), side, algorithm);
            execution_plan.print();
        } catch (...) {
            std::cerr << "Invalid input. Please enter a number or command.\n";
        }
    }
}