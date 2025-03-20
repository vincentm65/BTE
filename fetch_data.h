#ifndef FETCH_DATA_H
#define FETCH_DATA_H

#include <string>
#include <vector>
#include <algorithm>
#include <sqlite3.h>

std::string fetchStockData(const std::string& ticker, const std::string& initialAccessToken);

struct Candle {
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    int volume = 0;
    std::string ticker = "";
    std::string date = "";

    Candle() = default;
};

std::vector<Candle> parseCandles(const std::string& jsonResponse);
void insertCandlesToDB(sqlite3* DB, const std::vector<Candle>& candles);
void handleFetchStockData(sqlite3* DB, const std::string& ticker);

#endif // FETCH_DATA_H
