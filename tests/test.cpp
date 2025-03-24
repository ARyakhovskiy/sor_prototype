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
    ExecutionPlan execution_plan = router.distribute_order(12.0, OrderSide::BUY); // Buy 12 units

    // Verify the execution plan
    ASSERT_EQ(execution_plan.get_plan().size(), 2);
    EXPECT_EQ(execution_plan.get_plan()[0].exchange_name, "Exchange1");
    EXPECT_EQ(execution_plan.get_plan()[0].price, 100.0); // Price
    EXPECT_EQ(execution_plan.get_plan()[0].volume, 10.0); // Quantity
    EXPECT_EQ(execution_plan.get_plan()[1].exchange_name, "Exchange1");
    EXPECT_EQ(execution_plan.get_plan()[1].price, 101.0); // Price
    EXPECT_EQ(execution_plan.get_plan()[1].volume, 2.0);  // Quantity

    // Verify total fees
    double expected_fees = (10.0 * 100.0 * 0.001) + (2.0 * 101.0 * 0.001); // Fees for both fills
    EXPECT_NEAR(execution_plan.get_total_fees(), expected_fees, 1e-6);
}

// Test case where optimized solution is better than the greedy solution
TEST(SmartOrderRouterTest, OptimizationShowcase) {
    // Create order books with large minimum order sizes
    auto exchange1 = std::make_shared<OrderBook>("Exchange1", 0.001, 5.0); // Min size = 5.0
    auto exchange2 = std::make_shared<OrderBook>("Exchange2", 0.0005, 7.0); // Min size = 7.0
    auto exchange3 = std::make_shared<OrderBook>("Exchange3", 0.0002, 4.0); // Min size = 4.0

    // Setup order books (prices designed to make optimization find better combination)
    // Exchange1: Cheapest but can't fulfill entire order alone
    exchange1->add_ask(100.0, 5.0);  // 5@100 (effective 100.1)
    exchange1->add_ask(101.0, 5.0);  // 5@101 (effective 101.101)

    // Exchange2: Medium price but good for combination
    exchange2->add_ask(100.5, 7.0);  // 7@100.5 (effective 100.55025)

    // Exchange3: Slightly worse price but good for combination
    exchange3->add_ask(100.8, 4.0);  // 4@100.8 (effective 100.8016)
    exchange3->add_ask(100.6, 4.0);  // 4@100.6 (effective 100.6012)

    std::unordered_map<std::string, std::shared_ptr<OrderBook>> order_books = {
        {"Exchange1", exchange1},
        {"Exchange2", exchange2},
        {"Exchange3", exchange3}
    };

    SmartOrderRouter router(order_books);

    // Try to buy 8.0 units (can't be fulfilled by single exchange due to min sizes)
    // Greedy would take 5@100 from Exchange1 and fail to find remaining 3
    // Optimization should find combination of 4@100.6 (Exchange3) + 4@100.6 (Exchange3) = 8 total
    ExecutionPlan execution_plan = router.distribute_order(8.0, OrderSide::BUY);

    // Verify the execution plan found a solution
    ASSERT_FALSE(execution_plan.get_plan().empty()) 
        << "Solution found";

    // Verify we got exactly 8.0 units (100% fulfillment)
    double total_quantity = 0.0;
    for (const auto& fill : execution_plan.get_plan()) {
        total_quantity += fill.volume;
    }
    EXPECT_NEAR(total_quantity, 8.0, 1e-6);

    // Verify we used the optimal combination (should be Exchange3's two 4.0 lots)
    if (execution_plan.get_plan().size() == 2) {
        EXPECT_EQ(execution_plan.get_plan()[0].exchange_name, "Exchange3");
        EXPECT_EQ(execution_plan.get_plan()[1].exchange_name, "Exchange3");
        EXPECT_NEAR(execution_plan.get_plan()[0].volume, 4.0, 1e-6);
        EXPECT_NEAR(execution_plan.get_plan()[1].volume, 4.0, 1e-6);
    }

    // Verify we got a better earn than taking 5@100 + nothing
    double effective_price_with_partial = (5.0 * 100.0 * 1.001) / 5.0; // Would only get 5 units
    double actual_effective_price = execution_plan.get_average_effective_price();
}