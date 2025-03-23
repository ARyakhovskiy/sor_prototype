#include "gtest/gtest.h"
#include "../include/smartorderrouter.h"
#include "../include/orderbook.h"
#include <memory>

class SmartOrderRouterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create order books for testing
        auto exchange1 = std::make_shared<OrderBook>("Exchange1", 0.001, 0.001);
        auto exchange2 = std::make_shared<OrderBook>("Exchange2", 0.0005, 0.01);

        // Add bids and asks to the order books
        exchange1->add_bid(100.0, 1.0);
        exchange1->add_ask(101.0, 1.0);
        exchange2->add_bid(99.0, 2.0);
        exchange2->add_ask(102.0, 2.0);

        order_books = {
            {"Exchange1", exchange1},
            {"Exchange2", exchange2}
        };
    }

    std::unordered_map<std::string, std::shared_ptr<OrderBook>> order_books;
};
