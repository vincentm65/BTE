#ifndef BACKTEST_H
#define BACKTEST_H

#include <sqlite3.h>
#include <string>
#include <vector>
#include <ostream>
#include <iostream>

// Data class for loading stock data
class Data
{
public:
    struct Bar {
        double open;
        double high;
        double low;
        double close;
        int volume;
        std::string ticker;
        std::string date;
    };
};

// Indicator class
class Indicators
{
public:
    // Constructor that takes bar vector to perform calculations
    Indicators(const std::vector<Data::Bar>& bars);

    double movingAverage(int length, size_t endIndex);

private:
    std::vector<Data::Bar> bars_;
};

// Order class and enums
enum class OrderType { Market, Limit };
enum class OrderSide { Buy, Sell };

class Order {
public:
    // Constructor declaration
    Order(OrderType type, OrderSide side, double price, int quantity, std::string ticker)
        : type(type), side(side), price(price), quantity(quantity), ticker(ticker) {}

    // Getter functions for each private member
    OrderType getType() const { return type; }
    OrderSide getSide() const { return side; }
    double getPrice() const { return price; }
    int getQuantity() const { return quantity; }
    std::string getTicker() const { return ticker; }

    // Overloading the output operator for printing the order details
    friend std::ostream& operator<<(std::ostream& os, const Order& order) {
        os << "Order [Type: "
           << (order.type == OrderType::Limit ? "Limit" : "Market")
           << ", Side: "
           << (order.side == OrderSide::Buy ? "Buy" : "Sell")
           << ", Price: $" << order.price
           << ", Quantity: " << order.quantity
           << ", Ticker: " << order.ticker << "]";
        return os;
    }

private:
    OrderType type;
    OrderSide side;
    double price;
    int quantity;
    std::string ticker;
};

// Struct to record completed round trip trades
struct TradeRecord {
    std::string ticker;
    std::string buyDate;
    std::string sellDate;
    double buyPrice;
    double sellPrice;
};

// Backtest class to orchestrate the simulation
class Backtest
{
public:
    std::vector<TradeRecord> run(std::vector<Data::Bar>& bars);
};

#endif // BACKTEST_H
