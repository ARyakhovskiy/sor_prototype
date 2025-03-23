#include <gtest/gtest.h>
#include "../include/orderbook.h"
#include "../include/smartorderrouter.h"
#include <memory>

// Test scenario: Buy 12 units with 2 price levels
TEST(SmartOrderRouterTest, BuyOrderWithTwoPriceLevels) 
{
    // Create an order book for Binance
    auto binance = std::make_shared<OrderBook>("Binance", 0.001, 1.0); // 0.1% taker fee, 1.0 min order size

    // Add asks to the order book
    binance->add_ask(100.0, 10.0); // Price: $100, Volume: 10
    binance->add_ask(101.0, 10.0); // Price: $101, Volume: 10

    // Create a SmartOrderRouter with the order book
    std::unordered_map<std::string, std::shared_ptr<OrderBook>> order_books = {{"Binance", binance}};
    SmartOrderRouter router(order_books);

    // Define order parameters
    double order_size = 12.0; // Total quantity to buy
    bool is_buy = true;       // Buy order

    // Distribute the order across exchanges
    auto execution_plan = router.distribute_order(order_size, is_buy);

    // Verify the execution plan
    ASSERT_EQ(execution_plan.size(), 2); // Expect 2 fills

    // First fill: 10 units at $100
    EXPECT_EQ(execution_plan[0].first, "Binance");
    EXPECT_DOUBLE_EQ(execution_plan[0].second.first, 100.0); // Price
    EXPECT_DOUBLE_EQ(execution_plan[0].second.second, 10.0); // Quantity

    // Second fill: 2 units at $101
    EXPECT_EQ(execution_plan[1].first, "Binance");
    EXPECT_DOUBLE_EQ(execution_plan[1].second.first, 101.0); // Price
    EXPECT_DOUBLE_EQ(execution_plan[1].second.second, 2.0);  // Quantity
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}