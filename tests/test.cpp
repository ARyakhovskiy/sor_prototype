#include "gtest/gtest.h"
#include "smartorderrouter.h"
#include "orderbook.h"
#include "utils.h"
#include <memory>

class SmartOrderRouterTest : public ::testing::Test {
protected:
    void SetUp() override 
    {
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


// Test 1: Single Exchange, Two Price Levels
TEST(SmartOrderRouterTest, SingleExchangeTwoPriceLevels) {
    // Create an order book for a single exchange
    auto exchange1 = std::make_shared<OrderBook>("Exchange1", 0.001, 1.0); // 0.1% taker fee, min order size = 1.0

    // Add two price levels
    exchange1->add_ask(100.0, 10.0); // Price: 100.0, Volume: 10.0
    exchange1->add_ask(101.0, 10.0); // Price: 101.0, Volume: 10.0

    // Create the unordered_map of exchanges
    std::unordered_map<std::string, std::shared_ptr<OrderBook>> order_books = {
        {"Exchange1", exchange1}
    };

    // Create the SmartOrderRouter
    SmartOrderRouter router(order_books);

    // Distribute the order
    ExecutionPlan execution_plan = router.distribute_order(12.0, true); // Buy 12 units

    // Verify the execution plan
    ASSERT_EQ(execution_plan.get_plan().size(), 2);
    EXPECT_EQ(execution_plan.get_plan()[0].first, "Exchange1");
    EXPECT_EQ(execution_plan.get_plan()[0].second.first, 100.0); // Price
    EXPECT_EQ(execution_plan.get_plan()[0].second.second, 10.0); // Quantity
    EXPECT_EQ(execution_plan.get_plan()[1].first, "Exchange1");
    EXPECT_EQ(execution_plan.get_plan()[1].second.first, 101.0); // Price
    EXPECT_EQ(execution_plan.get_plan()[1].second.second, 2.0);  // Quantity

    // Verify total fees
    double expected_fees = (10.0 * 100.0 * 0.001) + (2.0 * 101.0 * 0.001); // Fees for both fills
    EXPECT_NEAR(execution_plan.get_total_fees(), expected_fees, 1e-6);
}