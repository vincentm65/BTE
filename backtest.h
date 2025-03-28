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
    double adr(int length, size_t endIndex);

private:
    std::vector<Data::Bar> bars_;
};

// Order class and enums
enum class OrderType { Market, Limit, BuyStopLimit, Stop };
enum class OrderSide { Buy, Sell };
enum class OrderInfo { Partial, Full };
enum class OrderStatus { Active, Filled, Canceled };

class Order {
public:
    // Constructor declaration
    Order(OrderType type, OrderSide side, double price, int quantity, std::string ticker, OrderInfo info, OrderStatus status)
        : type(type), side(side), price(price), quantity(quantity), ticker(ticker), info(info), status(status){}

    // Getter functions for each private member
    OrderType getType() const { return type; }
    OrderSide getSide() const { return side; }
    double getPrice() const { return price; }
    int getQuantity() const { return quantity; }
    std::string getTicker() const { return ticker; }
    OrderInfo getInfo() const { return info;}
    OrderStatus getStatus() const { return status; }

    // Create cancel call
    void cancel() { status = OrderStatus::Canceled; }

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
    OrderInfo info;
    OrderStatus status;
};

// Struct to record completed round trip trades
struct TradeRecord {
    std::string ticker;
    std::string buyDate;
    std::string sellDate;
    double buyPrice;
    double sellPrice;
    double quantity;
    std::string info;
};

// Backtest class to orchestrate the simulation
class Backtest
{
public:
    std::vector<TradeRecord> run(const std::vector<Data::Bar>& bars);
};

#endif // BACKTEST_H
