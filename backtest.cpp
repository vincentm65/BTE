#include "backtest.h"

#include <iostream>
#include <sqlite3.h>
#include <string>
#include <algorithm>

// Debug macro: define DEBUG_MODE to enable detailed logging; undefine for production
//#define DEBUG_MODE
#ifdef DEBUG_MODE
#define DEBUG_PRINT(x) std::cout << x << std::endl;
#else
#define DEBUG_PRINT(x)
#endif

// Data Class - Query stock data from the database with preallocation optimization
std::vector<Data::Bar> Data::queryStockForBT(sqlite3* DB, std::string ticker)
{
    // Convert ticker to uppercase to match DB data
    std::transform(ticker.begin(), ticker.end(), ticker.begin(), ::toupper);

    // --- Preallocate vector memory by first counting rows ---
    std::string countSQL = "SELECT COUNT(*) FROM Stocks WHERE ticker = ?;";
    sqlite3_stmt* countStmt = nullptr;
    int rowCount = 0;
    if(sqlite3_prepare_v2(DB, countSQL.c_str(), -1, &countStmt, NULL) == SQLITE_OK) {
        if(sqlite3_bind_text(countStmt, 1, ticker.c_str(), -1, SQLITE_STATIC) == SQLITE_OK) {
            if(sqlite3_step(countStmt) == SQLITE_ROW) {
                rowCount = sqlite3_column_int(countStmt, 0);
            }
        }
    }
    sqlite3_finalize(countStmt);

    // --- Fetch bar data ---
    std::string querySQL = "SELECT * FROM Stocks WHERE ticker = ?;";
    sqlite3_stmt* stmt = nullptr;
    std::vector<Bar> records;
    records.reserve(rowCount);  // Reserve capacity to avoid reallocation

    if (sqlite3_prepare_v2(DB, querySQL.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
        std::cerr << "Error preparing query: " << sqlite3_errmsg(DB) << std::endl;
        sqlite3_close(DB);
        return records;
    }

    if (sqlite3_bind_text(stmt, 1, ticker.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
        std::cerr << "Error binding parameter: " << sqlite3_errmsg(DB) << std::endl;
        sqlite3_finalize(stmt);
        sqlite3_close(DB);
        return records;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        double open = sqlite3_column_double(stmt, 0);
        double high = sqlite3_column_double(stmt, 1);
        double low = sqlite3_column_double(stmt, 2);
        double close = sqlite3_column_double(stmt, 3);
        int volume = sqlite3_column_int(stmt, 4);

        const unsigned char* tickerText = sqlite3_column_text(stmt, 5);
        const unsigned char* date = sqlite3_column_text(stmt, 6);

        Data::Bar record;
        record.open = open;
        record.high = high;
        record.low = low;
        record.close = close;
        record.volume = volume;
        if (tickerText) {
            record.ticker = reinterpret_cast<const char*>(tickerText);
        }
        if (date) {
            record.date = reinterpret_cast<const char*>(date);
        }
        records.push_back(record);
    }
    sqlite3_finalize(stmt);

    return records;
}

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

    // Structure to hold details for an open trade
    struct OpenTrade {
        double buyPrice;
        std::string buyDate;
        double takeProfit;
        double stopLoss;
    } openTrade;

    // Reserve estimated number of trades to reduce reallocations
    completedTrades.reserve(bars.size() / 5);

    // Moving average period; for example, use 10 bars.
    const int maPeriod = 10;


    // Process bars starting from the second bar
    for (size_t i = 1; i < bars.size(); ++i) {
        const Data::Bar& bar = bars[i];
        DEBUG_PRINT("\nProcessing bar on " << bar.date << " (Close: $" << bar.close << ")");

        // --- Calculate the Moving Average using the Indicators class ---

        double ma = indicators.movingAverage(maPeriod, i);
        DEBUG_PRINT("Moving Average (" << maPeriod << "): " << ma);


        // --- Order Management: Check pending buy order ---
        if (pendingBuyOrderActive) {
            if (bar.high >= pendingBuyOrder.getPrice()) {
                DEBUG_PRINT("Buy order filled at $" << pendingBuyOrder.getPrice() << " on " << bar.date);
                inPosition = true;
                openTrade.buyPrice = pendingBuyOrder.getPrice();
                openTrade.buyDate = bar.date;
                openTrade.takeProfit = openTrade.buyPrice + 5.0;
                openTrade.stopLoss = openTrade.buyPrice - 1.0;
                DEBUG_PRINT("Placed sell orders: Take-Profit at $" << openTrade.takeProfit
                                                                   << " and Stop-Loss at $" << openTrade.stopLoss);
                pendingBuyOrderActive = false;
            }
        }
        // --- New Buy Signal (only if not already in a position) ---
        else if (!inPosition) {
            const Data::Bar& previousBar = bars[i - 1];
            if (bar.high > ma) {
                double orderPrice = ma;  // Buy stop-limit order price set above market price
                pendingBuyOrder = Order(OrderType::Limit, OrderSide::Buy, orderPrice, 1, bar.ticker);
                pendingBuyOrderActive = true;
                DEBUG_PRINT("Buy order placed: " << pendingBuyOrder << " on " << bar.date);
            }
        }

        // --- Position Tracking and Sell Orders ---
        if (inPosition) {
            if (bar.high >= openTrade.takeProfit) {
                DEBUG_PRINT("Take-Profit order filled at $" << openTrade.takeProfit << " on " << bar.date);
                TradeRecord trade;
                trade.ticker = bar.ticker;
                trade.buyDate = openTrade.buyDate;
                trade.sellDate = bar.date;
                trade.buyPrice = openTrade.buyPrice;
                trade.sellPrice = openTrade.takeProfit;
                completedTrades.push_back(trade);
                DEBUG_PRINT("Cancelled Stop-Loss order for trade opened on " << openTrade.buyDate);
                inPosition = false;
            }
            else if (bar.low <= openTrade.stopLoss) {
                DEBUG_PRINT("Stop-Loss order filled at $" << openTrade.stopLoss << " on " << bar.date);
                TradeRecord trade;
                trade.ticker = bar.ticker;
                trade.buyDate = openTrade.buyDate;
                trade.sellDate = bar.date;
                trade.buyPrice = openTrade.buyPrice;
                trade.sellPrice = openTrade.stopLoss;
                completedTrades.push_back(trade);
                DEBUG_PRINT("Cancelled Take-Profit order for trade opened on " << openTrade.buyDate);
                inPosition = false;
            }
        }
    }
    return completedTrades;
}
