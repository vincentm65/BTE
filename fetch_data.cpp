#include <iostream>
#include <fstream>
#include <sstream>
#include <sqlite3.h>
#include <string>
#include <vector>
#include <algorithm>
#include <curl/curl.h>
#include <json/json.h>
#include <ctime>
#include <QDebug>  // Added for QDebug logging

#include "authenticate.h"
#include "fetch_data.h"

// ------------------------
// Inserting New Data
// ------------------------

std::string fetchStockData(const std::string& ticker, const std::string& initialAccessToken) {
    CURL* curl;
    CURLcode res;
    long httpCode = 0;
    std::string readBuffer;
    std::string url = "https://api.schwabapi.com/marketdata/v1/pricehistory?symbol="
                      + ticker + "&periodType=month&period=1&frequencyType=daily&frequency=1";

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (!curl) {
        std::cerr << "CURL initialization failed!" << std::endl;
        return "";
    }

    // We'll use this flag to ensure we only attempt a refresh once.
    bool attemptedRefresh = false;
    // Start with the provided access token.
    std::string accessToken = initialAccessToken;

    do {
        // Clear the buffer for each attempt.
        readBuffer.clear();

        // Set up the request with the current token.
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + accessToken).c_str());
        headers = curl_slist_append(headers, "accept: application/json");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        // Perform the request.
        res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_slist_free_all(headers);

        if (res != CURLE_OK) {
            std::cerr << "CURL Error: " << curl_easy_strerror(res) << std::endl;
            readBuffer.clear();
            break;
        }

        if (httpCode == 401 && !attemptedRefresh) {
            std::cerr << "HTTP 401 Unauthorized: Access token may be expired. Attempting to refresh token..." << std::endl;

            // Read the refresh token from tokens.txt
            std::ifstream tokenFile("tokens.txt");
            std::string line, refreshToken;
            if (tokenFile.is_open()) {
                while (std::getline(tokenFile, line)) {
                    if (line.find("refresh_token=") == 0) {
                        refreshToken = line.substr(line.find("=") + 1);
                        break;
                    }
                }
                tokenFile.close();
            }

            if (!refreshToken.empty()) {
                // Call refreshAccessToken with the refresh token to get a new access token.
                std::string newToken = refreshAccessToken(refreshToken);
                if (!newToken.empty()) {
                    accessToken = newToken;
                    attemptedRefresh = true;
                    // Retry the API call with the new access token.
                    continue;
                }
                else {
                    std::cerr << "Token refresh failed." << std::endl;
                    break;
                }
            }
            else {
                std::cerr << "No refresh token found in tokens.txt." << std::endl;
                break;
            }
        }
        // If we didn't get a 401 (or we've already attempted refresh), exit the loop.
        break;
    } while (true);

    if (httpCode != 200) {
        std::cerr << "HTTP Error: " << httpCode << " - " << readBuffer << std::endl;
        readBuffer.clear();
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return readBuffer;
}

std::vector<Candle> parseCandles(const std::string& jsonResponse) {
    std::vector<Candle> candlesVector;

    // Parse JSON using JsonCpp
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    std::istringstream iss(jsonResponse);
    if (!Json::parseFromStream(builder, iss, &root, &errs)) {
        std::cerr << "Error parsing JSON: " << errs << std::endl;
        return candlesVector;
    }

    // Extract the ticker symbol from the root object
    std::string ticker = root["symbol"].asString();

    // Iterate over the candles array
    const Json::Value& candles = root["candles"];
    for (unsigned int i = 0; i < candles.size(); ++i) {
        Candle c;
        c.open = candles[i]["open"].asDouble();
        c.high = candles[i]["high"].asDouble();
        c.low = candles[i]["low"].asDouble();
        c.close = candles[i]["close"].asDouble();
        c.volume = candles[i]["volume"].asInt();
        c.ticker = ticker;

        // Convert epoch milliseconds to a date string ("YYYY-MM-DD")
        long long dt_ms = candles[i]["datetime"].asLargestInt();
        time_t dt_sec = static_cast<time_t>(dt_ms / 1000);
        struct tm timeinfo;
#ifdef _WIN32
        localtime_s(&timeinfo, &dt_sec);
#else
        localtime_r(&dt_sec, &timeinfo);
#endif
        char buf[11];  // "YYYY-MM-DD" plus null terminator
        strftime(buf, sizeof(buf), "%Y-%m-%d", &timeinfo);
        c.date = buf;

        candlesVector.push_back(c);
    }

    return candlesVector;
}

void insertCandlesToDB(sqlite3* DB, const std::vector<Candle>& candles) {
    sqlite3_stmt* stmt;
    const std::string insertSQL =
        "INSERT OR IGNORE INTO Stocks (open, high, low, close, volume, ticker, date) VALUES (?, ?, ?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(DB, insertSQL.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Error preparing insert statement: " << sqlite3_errmsg(DB) << std::endl;
        return;
    }

    // Begin a transaction for efficiency.
    sqlite3_exec(DB, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    for (const auto& candle : candles) {
        sqlite3_bind_double(stmt, 1, candle.open);
        sqlite3_bind_double(stmt, 2, candle.high);
        sqlite3_bind_double(stmt, 3, candle.low);
        sqlite3_bind_double(stmt, 4, candle.close);
        sqlite3_bind_int(stmt, 5, candle.volume);
        sqlite3_bind_text(stmt, 6, candle.ticker.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, candle.date.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "Error inserting candle data: " << sqlite3_errmsg(DB) << std::endl;
        }
        sqlite3_reset(stmt);
    }

    sqlite3_exec(DB, "COMMIT;", nullptr, nullptr, nullptr);
    sqlite3_finalize(stmt);
    std::cout << "Stock data inserted into the database successfully!" << std::endl;
}

void handleFetchStockData(sqlite3* DB, const std::string& ticker) {
    std::string accessToken = getAccessToken();

    if (accessToken.empty()) {
        std::cerr << "Error: Access token not available." << std::endl;
        return;
    }

    std::string response = fetchStockData(ticker, accessToken);
    // Added QDebug statement to log the raw API response
    qDebug() << "API Response:" << QString::fromStdString(response);

    if (response.empty()) {
        std::cerr << "No data received from the API.\n";
    } else {

        std::vector<Candle> candles = parseCandles(response);
        std::ostringstream outputStream;
        outputStream  << "Fetched data for ticker: " << ticker << "\n--------------------------------------\n";
        outputStream  << "Open\tHigh\tLow\tClose\tVolume\tTicker\tDate\n";
        for (const auto& c : candles) {
            if (c.close == 0.0) {
                outputStream << "Warning: Uninitialized candle detected!\n";
            }
            outputStream << c.open << "\t"
                         << c.high << "\t"
                         << c.low << "\t"
                         << c.close << "\t"
                         << c.volume << "\t"
                         << c.ticker << "\t"
                         << c.date << "\n";
        }
        insertCandlesToDB(DB, candles);
        std::cout << outputStream.str();  // This will be captured in MainWindow
    }
}
