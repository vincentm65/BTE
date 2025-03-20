#include "manage_db.h"

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
#include <thread>
#include <chrono>

#include <QTextEdit>
#include <QCoreApplication>

#include "authenticate.h"
#include "fetch_data.h"

// ------------------------
// Database Functions
// ------------------------

void removeDuplicates(sqlite3* DB) {
    const char* deleteSQL =
        "WITH Duplicates AS ("
        "    SELECT rowid FROM Stocks "
        "    WHERE rowid NOT IN ("
        "        SELECT MIN(rowid) FROM Stocks GROUP BY ticker, date"
        "    )"
        ") "
        "DELETE FROM Stocks WHERE rowid IN (SELECT rowid FROM Duplicates);";

    char* errorMessage;
    int exit = sqlite3_exec(DB, deleteSQL, NULL, NULL, &errorMessage);
    if (exit != SQLITE_OK) {
        std::cerr << "Error removing duplicates: " << errorMessage << std::endl;
        sqlite3_free(errorMessage);
    }
    else {
        std::cout << "Duplicates removed successfully!" << std::endl;
    }
}


void createDatabase(sqlite3* DB) {
    char* errorMessage;
    int exit = sqlite3_open("Universe_OHLCV.db", &DB);
    if (exit) {
        std::cerr << "Error opening database: " << sqlite3_errmsg(DB) << std::endl;
        return;
    }
    std::cout << "Database opened successfully!" << std::endl;

    std::string createTableSQL = "CREATE TABLE IF NOT EXISTS Stocks ("
                                 "open REAL, "
                                 "high REAL, "
                                 "low REAL, "
                                 "close REAL, "
                                 "volume INTEGER,"
                                 "ticker TEXT, "
                                 "date TEXT);";
    exit = sqlite3_exec(DB, createTableSQL.c_str(), NULL, 0, &errorMessage);
    if (exit != SQLITE_OK) {
        std::cerr << "Error creating table: " << errorMessage << std::endl;
        sqlite3_free(errorMessage);
    }
    else {
        std::cout << "Table created successfully!" << std::endl;
    }

    std::ifstream file("C:\\Users\\Vincents XPS\\Desktop\\Scripts\\Project\\universe_price_history.csv");
    if (!file.is_open()) {
        std::cerr << "Error opening CSV file!" << std::endl;
    }
    std::string line;
    std::getline(file, line);

    sqlite3_stmt* stmt;
    std::string insertSQL = "INSERT INTO Stocks (open, high, low, close, volume, ticker, date) VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_prepare_v2(DB, insertSQL.c_str(), -1, &stmt, NULL);

    std::string ticker, date;
    double open, high, low, close;
    int volume;

    sqlite3_exec(DB, "PRAGMA synchronous = OFF;", NULL, NULL, NULL);
    sqlite3_exec(DB, "PRAGMA journal_mode = MEMORY;", NULL, NULL, NULL);
    sqlite3_exec(DB, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        ss >> open; ss.ignore();
        ss >> high; ss.ignore();
        ss >> low; ss.ignore();
        ss >> close; ss.ignore();
        ss >> volume; ss.ignore();
        std::getline(ss, ticker, ',');
        std::getline(ss, date);
        sqlite3_bind_double(stmt, 1, open);
        sqlite3_bind_double(stmt, 2, high);
        sqlite3_bind_double(stmt, 3, low);
        sqlite3_bind_double(stmt, 4, close);
        sqlite3_bind_int(stmt, 5, volume);
        sqlite3_bind_text(stmt, 6, ticker.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, date.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "Error inserting data: " << sqlite3_errmsg(DB) << std::endl;
        }
        sqlite3_reset(stmt);
    }

    sqlite3_exec(DB, "COMMIT;", NULL, NULL, NULL);
    sqlite3_finalize(stmt);
    file.close();
    sqlite3_close(DB);
    std::cout << "CSV data inserted into SQLite successfully!" << std::endl;
}

void queryStock(sqlite3* DB, std::string ticker) {
    std::transform(ticker.begin(), ticker.end(), ticker.begin(), ::toupper);

    std::string querySQL = "SELECT * FROM Stocks WHERE ticker = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(DB, querySQL.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
        std::cerr << "Error preparing query: " << sqlite3_errmsg(DB) << std::endl;
        sqlite3_close(DB);
        return;
    }
    sqlite3_bind_text(stmt, 1, ticker.c_str(), -1, SQLITE_STATIC);
    std::cout << "Results for ticker: " << ticker << "\n--------------------------------------\n";
    std::cout << "Open\tHigh\tLow\tClose\tVolume\tTicker\tDate\n";
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        double open = sqlite3_column_double(stmt, 0);
        double high = sqlite3_column_double(stmt, 1);
        double low = sqlite3_column_double(stmt, 2);
        double close = sqlite3_column_double(stmt, 3);
        int volume = sqlite3_column_int(stmt, 4);
        const unsigned char* tickerResult = sqlite3_column_text(stmt, 5);
        const unsigned char* date = sqlite3_column_text(stmt, 6);
        std::cout << open << "\t" << high << "\t" << low << "\t" << close
                  << "\t" << volume << "\t" << tickerResult << "\t" << date << "\n";
    }
    sqlite3_finalize(stmt);
}

void updatePriceHistory(sqlite3* DB, QTextEdit* outputWidget) {
    std::ifstream file("C:\\BTE\\universe.csv");
    if (!file.is_open()) {
        outputWidget->append("Error opening CSV file!");
        return;
    }

    std::vector<std::string> tickers((std::istream_iterator<std::string>(file)), std::istream_iterator<std::string>());

    file.close();

    std::vector<Candle> allCandles;
    std::ostringstream outputStream;

    // Print tickers
    for (const auto& ticker : tickers) {
        outputWidget->append(QString::fromStdString("Querying: " + ticker));
        std::string accessToken = getAccessToken();
        std::string response = fetchStockData(ticker, accessToken);

        if (response.empty()) {
            outputWidget->append(QString::fromStdString("No data found for " + ticker));
        }
        else {
            //outputWidget->append(QString::fromStdString("Fetching " + ticker));
            QCoreApplication::processEvents();

            std::vector<Candle> candles = parseCandles(response);
            allCandles.insert(allCandles.end(), candles.begin(), candles.end());

        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (!allCandles.empty()) {
         outputWidget->append(QString::fromStdString("Inserting " + std::to_string(allCandles.size()) + " rows."));
        insertCandlesToDB(DB, allCandles);
    }
    removeDuplicates(DB);
}

