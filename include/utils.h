#ifndef UTILS_H
#define UTILS_H

#include "orderbook.h"
#include <string>

// Reads CSV data into an OrderBook
void read_csv(const std::string& filename, OrderBook& order_book);

#endif // UTILS_H