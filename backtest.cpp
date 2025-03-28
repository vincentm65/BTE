#include "backtest.h"

#include <string>

Indicators::Indicators(const std::vector<Data::Bar>& bars) : bars_(bars) {}

double Indicators::movingAverage(int length, size_t endIndex) {
    if (bars_.size() < static_cast<size_t>(length)) {
        return 0;
    }

    double sum = 0.0;
    for (size_t i = endIndex + 1 - length; i <= endIndex; ++i) {
        sum += bars_[i].close;
    }
    return sum / length;
}

double Indicators::adr(int length, size_t endIndex) {
    if (endIndex + 1 < static_cast<size_t>(length)) {
        return 0;
    }

    double sumADR = 0.0;
    for (size_t i = endIndex + 1 - length; i <= endIndex; ++i) {
        sumADR += (bars_[i].high - bars_[i].low);
    }

    return sumADR / length;
}

double Indicators::avgVolume(int length, size_t endIndex) {
    if (endIndex + 1 < static_cast<size_t>(length)) {
        return 0;
    }

    double sumVol = 0.0;
    for (size_t i = endIndex + 1 - length; i <= endIndex; ++i) {
        sumVol += (bars_[i].volume);
    }

    return sumVol / length;
}

std::vector<TradeRecord> Backtest::run(const std::vector<Data::Bar>& bars) {
    std::vector<TradeRecord> completedTrades;
    if (bars.size() < 2) {
        return completedTrades;
    }

    std::string ticker = bars.front().ticker;
    Indicators indicators(bars);

    Order pBuyStopOrder(OrderType::Limit, OrderSide::Buy, 0.0, 0, ticker, OrderInfo::Full, OrderStatus::Canceled);
    Order pStopOrder(OrderType::Stop, OrderSide::Sell, 0.0, 0, ticker, OrderInfo::Full, OrderStatus::Canceled);
    Order pLimitPartialOrder(OrderType::Limit, OrderSide::Sell, 0.0, 0, ticker, OrderInfo::Partial, OrderStatus::Canceled);
    Order pLimitFlatOrder(OrderType::Limit, OrderSide::Sell, 0.0, 0, ticker, OrderInfo::Full, OrderStatus::Canceled);

    int dollarRisk = 100;
    bool pBuyStopOrderActive = false;
    bool inPosition = false;
    bool partialHit = false;
    const int maPeriod = 10;

    struct OpenTrade {
        double buyPrice;
        std::string buyDate;
        double takeProfit;
        double partialTarget;
        int quantity;
        double stopLoss;
    } openTrade;

    completedTrades.reserve(bars.size() / 5);

    for (size_t i = 14; i < bars.size(); ++i) {
        const Data::Bar& bar = bars[i];
        if (i < static_cast<size_t>(maPeriod - 1)) {
            continue;
        }

        double ma = indicators.movingAverage(maPeriod, i);
        double adr = indicators.adr(14, i);
        double volume = indicators.avgVolume(14, i);

        if (pBuyStopOrderActive) {
            if (bar.high >= pBuyStopOrder.getPrice()) {
                inPosition = true;
                openTrade.buyPrice = pBuyStopOrder.getPrice();
                openTrade.buyDate = bar.date;
                openTrade.quantity = pBuyStopOrder.getQuantity();

                double stopPrice = openTrade.buyPrice - adr;
                double partialPrice = openTrade.buyPrice + adr;
                double takeProfitPrice = openTrade.buyPrice + (3 * adr);

                pStopOrder = Order(OrderType::Stop, OrderSide::Sell, stopPrice, openTrade.quantity, bar.ticker, OrderInfo::Full, OrderStatus::Active);
                pLimitPartialOrder = Order(OrderType::Limit, OrderSide::Sell, partialPrice, openTrade.quantity / 2, bar.ticker, OrderInfo::Partial, OrderStatus::Active);
                pLimitFlatOrder = Order(OrderType::Limit, OrderSide::Sell, takeProfitPrice, openTrade.quantity / 2, bar.ticker, OrderInfo::Full, OrderStatus::Active);

                pBuyStopOrderActive = false;
            }
        } else if (!inPosition && pBuyStopOrderActive == false) {
            const Data::Bar& previousBar = bars[i - 1];
            if (bar.high > ma, volume > 50000000) {
                int orderSize = dollarRisk / adr;
                if (orderSize >= 2) {
                    double orderPrice = ma + (adr * 2);
                    pBuyStopOrder = Order(OrderType::BuyStopLimit, OrderSide::Buy, orderPrice, orderSize, bar.ticker, OrderInfo::Full, OrderStatus::Active);
                    pBuyStopOrderActive = true;
                }
            }
        }

        if (inPosition) {
            if (bar.high >= pLimitFlatOrder.getPrice() && partialHit == true) {
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
            } else if (bar.high >= pLimitPartialOrder.getPrice() && partialHit == false) {
                TradeRecord partialTrade;
                partialTrade.ticker = bar.ticker;
                partialTrade.buyDate = openTrade.buyDate;
                partialTrade.sellDate = bar.date;
                partialTrade.buyPrice = openTrade.buyPrice;
                partialTrade.quantity = pLimitPartialOrder.getQuantity();
                partialTrade.sellPrice = pLimitPartialOrder.getPrice();
                partialTrade.info = "Partial";
                completedTrades.push_back(partialTrade);
                pStopOrder.cancel();
                openTrade.quantity -= pLimitPartialOrder.getQuantity();
                pStopOrder = Order(OrderType::Stop, OrderSide::Sell, openTrade.buyPrice, openTrade.quantity, bar.ticker, OrderInfo::Full, OrderStatus::Active);
                pLimitPartialOrder.cancel();
                partialHit = true;
            } else if (bar.low <= pStopOrder.getPrice()) {
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
