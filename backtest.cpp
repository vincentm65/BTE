#include "backtest.h"

#include <iostream>
#include <string>
#include <qDebug>

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

double Indicators::adr(int length, size_t endIndex) {
    if (endIndex + 1 < static_cast<size_t>(length)) {
        std::cerr << "Not enough data to compute ADR over " << length << " days." << std::endl;
        return 0;
    }

    double sumADR = 0.0;
    for (size_t i = endIndex + 1 - length; i <= endIndex; ++i) {
        double dailyRange = (bars_[i].high - bars_[i].low);
        sumADR += dailyRange;
    }

    return sumADR / length;
}


// Backtest Class - Run simulation with order management and trade tracking
std::vector<TradeRecord> Backtest::run(const std::vector<Data::Bar>& bars)
{
    std::vector<TradeRecord> completedTrades;

    if (bars.size() < 2) {
        std::cerr << "Not enough data to run backtest." << std::endl;
        return completedTrades;
    }

    std::string ticker = bars.front().ticker;

    Indicators indicators(bars);

    // Order management and trade tracking variables
    Order pBuyStopOrder(OrderType::Limit, OrderSide::Buy, 0.0, 0, ticker, OrderInfo::Full, OrderStatus::Canceled);
    Order pStopOrder(OrderType::Stop, OrderSide::Sell, 0.0, 0, ticker, OrderInfo::Full, OrderStatus::Canceled);
    Order pLimitPartialOrder(OrderType::Limit, OrderSide::Sell, 0.0, 0, ticker, OrderInfo::Partial, OrderStatus::Canceled);
    Order pLimitFlatOrder(OrderType::Limit, OrderSide::Sell, 0.0, 0, ticker, OrderInfo::Full, OrderStatus::Canceled);

    int dollarRisk = 100;
    bool pBuyStopOrderActive = false;
    bool inPosition = false;
    bool partialHit = false;

    // Set indicator lengths
    const int maPeriod = 10;

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

    // Process bars starting from the 14th bar
    for (size_t i = 14; i < bars.size(); ++i) {
        const Data::Bar& bar = bars[i];
        qDebug() << "Processing bar index:" << i
                 << "Date:" << QString::fromStdString(bar.date)
                 << "High:" << bar.high
                 << "Low:" << bar.low;

        if (i < static_cast<size_t>(maPeriod - 1)) {
            continue;
        }

        // Calculate the Moving Average and ADR using the Indicators class
        double ma = indicators.movingAverage(maPeriod, i);
        double adr = indicators.adr(14, i);
        qDebug() << "Calculated MA:" << ma << "ADR:" << adr;

        // Order Management: Check pending buy order
        if (pBuyStopOrderActive) {
            qDebug() << "Buy stop order active. Bar high:" << bar.high
                     << "vs Order price:" << pBuyStopOrder.getPrice();
            if (bar.high >= pBuyStopOrder.getPrice()) {
                // Set trade variables and flags
                inPosition = true;
                openTrade.buyPrice = pBuyStopOrder.getPrice();
                openTrade.buyDate = bar.date;
                openTrade.quantity = pBuyStopOrder.getQuantity();
                qDebug() << "Buy stop order hit. Entering position at price:" << openTrade.buyPrice;

                // Set prices for supporting sell orders
                double stopPrice = openTrade.buyPrice - adr;
                double partialPrice = openTrade.buyPrice + adr;
                double takeProfitPrice = openTrade.buyPrice + (3 * adr);
                qDebug() << "Setting stopPrice:" << stopPrice
                         << "PartialPrice:" << partialPrice
                         << "TakeProfitPrice:" << takeProfitPrice;

                // Create supporting sell orders
                pStopOrder = Order(OrderType::Stop, OrderSide::Sell, stopPrice, openTrade.quantity, bar.ticker, OrderInfo::Full, OrderStatus::Active);
                pLimitPartialOrder = Order(OrderType::Limit, OrderSide::Sell, partialPrice, openTrade.quantity / 2, bar.ticker, OrderInfo::Partial, OrderStatus::Active);
                pLimitFlatOrder = Order(OrderType::Limit, OrderSide::Sell, takeProfitPrice, openTrade.quantity / 2, bar.ticker, OrderInfo::Full, OrderStatus::Active);

                pBuyStopOrderActive = false;
            }
        }
        // New Buy Signal (only if not already in a position and no buy order pending)
        else if (!inPosition && pBuyStopOrderActive == false) {
            const Data::Bar& previousBar = bars[i - 1];
            if (bar.high > ma) {
                qDebug() << "New buy signal: bar high (" << bar.high << ") > MA (" << ma << ")";
                // Set position size
                int orderSize = dollarRisk / adr;
                qDebug() << "Calculated order size:" << orderSize << "with dollarRisk:" << dollarRisk << "and ADR:" << adr;

                if (orderSize >= 2) {
                    // Set order price
                    double orderPrice = ma + (adr * 0.25);
                    qDebug() << "Placing buy stop order at price:" << orderPrice << "with size:" << orderSize;
                    pBuyStopOrder = Order(OrderType::BuyStopLimit, OrderSide::Buy, orderPrice, orderSize, bar.ticker, OrderInfo::Full, OrderStatus::Active);
                    pBuyStopOrderActive = true;
                }
            }
        }

        // Position Tracking and Sell Orders
        if (inPosition) {
            if (bar.high >= pLimitFlatOrder.getPrice() && partialHit == true) {
                qDebug() << "Full exit condition met. Bar high:" << bar.high
                         << ">= Limit Flat Order Price:" << pLimitFlatOrder.getPrice()
                         << "and partial hit is true.";
                // Record all trade details to close out a full round trip trade.
                TradeRecord trade;
                trade.ticker = bar.ticker;
                trade.buyDate = openTrade.buyDate;
                trade.sellDate = bar.date;
                trade.buyPrice = openTrade.buyPrice;
                trade.sellPrice = pLimitFlatOrder.getPrice();
                trade.quantity = pLimitFlatOrder.getQuantity();
                trade.info = "Full";
                completedTrades.push_back(trade);
                pStopOrder.cancel();
                pLimitFlatOrder.cancel();

                partialHit = false;
                inPosition = false;
            }
            else if (bar.high >= pLimitPartialOrder.getPrice() && partialHit == false) {
                qDebug() << "Partial exit condition met. Bar high:" << bar.high
                         << ">= Limit Partial Order Price:" << pLimitPartialOrder.getPrice()
                         << "and partial hit is false.";
                TradeRecord partialTrade;
                partialTrade.ticker = bar.ticker;
                partialTrade.buyDate = openTrade.buyDate;
                partialTrade.sellDate = bar.date;
                partialTrade.buyPrice = openTrade.buyPrice;
                partialTrade.quantity = pLimitPartialOrder.getQuantity();
                partialTrade.sellPrice = pLimitPartialOrder.getPrice();
                partialTrade.info = "Partial";
                completedTrades.push_back(partialTrade);
                qDebug() << "Partial trade recorded. Updating stop order to open trade buy price:" << openTrade.buyPrice;
                // Cancel stop, ammend quantity, resend stop at entry price
                pStopOrder.cancel();
                openTrade.quantity -= pLimitPartialOrder.getQuantity();
                pStopOrder = Order(OrderType::Stop, OrderSide::Sell, openTrade.buyPrice, openTrade.quantity, bar.ticker, OrderInfo::Full, OrderStatus::Active);

                // Cancel the partial order once filled
                pLimitPartialOrder.cancel();
                partialHit = true;
            }
            else if (bar.low <= pStopOrder.getPrice()) {
                qDebug() << "Stop loss condition met. Bar low:" << bar.low
                         << "<= Stop Order Price:" << pStopOrder.getPrice();
                TradeRecord trade;
                trade.ticker = bar.ticker;
                trade.buyDate = openTrade.buyDate;
                trade.sellDate = bar.date;
                trade.buyPrice = openTrade.buyPrice;
                trade.sellPrice = pStopOrder.getPrice();
                trade.quantity = pStopOrder.getQuantity();
                trade.info = "Stop";
                completedTrades.push_back(trade);
                pLimitPartialOrder.cancel();
                pLimitFlatOrder.cancel();

                partialHit = false;
                inPosition = false;
            }
        }
    }
    return completedTrades;
}
