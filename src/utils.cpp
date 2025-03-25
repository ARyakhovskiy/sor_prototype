#include "utils.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

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

    while (std::getline(file, line)) 
    {
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