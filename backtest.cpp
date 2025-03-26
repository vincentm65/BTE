#include "backtest.h"

#include <iostream>
#include <sqlite3.h>
#include <string>
#include <algorithm>

Indicators::Indicators(const std::vector<Data::Bar>& bars) : bars_(bars) {}

double Indicators::movingAverage(int length, size_t endIndex) {
    // Check if there are enough bars to calculate the average.
    if (bars_.size() < static_cast<size_t>(length)) {
        std::cerr << "Not enough data to compute moving average." << std::endl;
        return 0;
    }

    double sum = 0.0;
    // Sum the close prices for the last 'length' bars.
    for (size_t i = endIndex + 1 - length; i <= endIndex; ++i) {
        sum += bars_[i].close;
    }
    double avg = sum / length;
    return (avg);
}

double Indicators::adr(int length) {

}


// Backtest Class - Run simulation with order management and trade tracking
std::vector<TradeRecord> Backtest::run(std::vector<Data::Bar>& bars)
{
    std::vector<TradeRecord> completedTrades;

    if (bars.size() < 2) {
        std::cerr << "Not enough data to run backtest." << std::endl;
        return completedTrades;
    }

    std::string ticker = bars.front().ticker;

    Indicators indicators(bars);

    // Order management and trade tracking variables
    bool pendingBuyOrderActive = false;
    Order pendingBuyOrder(OrderType::Limit, OrderSide::Buy, 0.0, 0, ticker); // Placeholder order
    bool inPosition = false;
    bool partialHit = false;


    // Structure to hold details for an open trade
    struct OpenTrade {
        double buyPrice;
        std::string buyDate;
        double takeProfit;
        double partialTarget;
        int quantity;
        double stopLoss;
    } openTrade;

    // Reserve estimated number of trades to reduce reallocations
    completedTrades.reserve(bars.size() / 5);

    // Moving average period; for example, use 10 bars.
    const int maPeriod = 10;


    // Process bars starting from the second bar
    for (size_t i = 1; i < bars.size(); ++i) {
        const Data::Bar& bar = bars[i];

        if (i < static_cast<size_t>(maPeriod - 1)) {
            continue;
        }

        // --- Calculate the Moving Average using the Indicators class ---

        double ma = indicators.movingAverage(maPeriod, i);

        // --- Order Management: Check pending buy order ---
        if (pendingBuyOrderActive) {
            // Buy logic for backtesting only, in practice the order will be set and hit
            if (bar.high >= pendingBuyOrder.getPrice()) {
                inPosition = true;
                openTrade.buyPrice = pendingBuyOrder.getPrice();
                openTrade.buyDate = bar.date;

                // We want to get the price by calling a funciton that returns a price
                openTrade.takeProfit = openTrade.buyPrice + 5.0;
                openTrade.partialTarget = openTrade.buyPrice + 2.5;
                openTrade.stopLoss = openTrade.buyPrice - 1.0;

                // Capture the total quantity purchased
                openTrade.quantity = pendingBuyOrder.getQuantity();

                pendingBuyOrderActive = false;
            }
        }
        // --- New Buy Signal (only if not already in a position) ---
        else if (!inPosition) {
            const Data::Bar& previousBar = bars[i - 1];
            if (bar.high > ma) {

                // Determine position size
                // need ADR
                double orderPrice = ma;  // Buy stop-limit order price set above market price
                pendingBuyOrder = Order(OrderType::Limit, OrderSide::Buy, orderPrice, 10, bar.ticker);
                pendingBuyOrderActive = true;
            }
        }

        // --- Position Tracking and Sell Orders ---
        if (inPosition) {
            if (bar.high >= openTrade.takeProfit && partialHit == true) {
                TradeRecord trade;
                trade.ticker = bar.ticker;
                trade.buyDate = openTrade.buyDate;
                trade.sellDate = bar.date;
                trade.buyPrice = openTrade.buyPrice;
                trade.sellPrice = openTrade.takeProfit;
                trade.quantity = openTrade.quantity;
                completedTrades.push_back(trade);
                inPosition = false;
                partialHit = false;
            }
            else if (bar.high >= openTrade.partialTarget && partialHit == false) {
                // Set percentage of first partial
                double partialPercent = 0.5;

                TradeRecord partialTrade;
                partialTrade.ticker = bar.ticker;
                partialTrade.buyDate = openTrade.buyDate;
                partialTrade.sellDate = bar.date;
                partialTrade.buyPrice = openTrade.buyPrice;
                partialTrade.quantity = openTrade.quantity * partialPercent;
                partialTrade.sellPrice = openTrade.partialTarget;
                completedTrades.push_back(partialTrade);

                // Reduce the open position quantity
                openTrade.quantity -= openTrade.quantity * partialPercent;

                partialHit = true;
            }
            else if (bar.low <= openTrade.stopLoss) {
                TradeRecord trade;
                trade.ticker = bar.ticker;
                trade.buyDate = openTrade.buyDate;
                trade.sellDate = bar.date;
                trade.buyPrice = openTrade.buyPrice;
                trade.sellPrice = openTrade.stopLoss;
                trade.quantity = openTrade.quantity;
                completedTrades.push_back(trade);
                inPosition = false;
                partialHit = false;
            }
        }
    }
    return completedTrades;
}
