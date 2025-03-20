#include "backtest_engine.h"
#include "ui_backtest_engine.h"
#include "backtest.h"

#include <QFile>
#include <QTextStream>
#include <QFileDialog>
#include <QDebug>
#include <QString>
#include <QRegularExpression>
#include <QPushButton>
#include <sqlite3.h>
#include <iostream>

backtest_engine::backtest_engine(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::backtest_engine)
    , db(nullptr)
{
    ui->setupUi(this);
    connect(ui->loadTickersButton, &QPushButton::clicked, this, &backtest_engine::loadTickersButton_Clicked);
    connect(ui->runBacktestButton, &QPushButton::clicked, this, &backtest_engine::runBacktestButton_Clicked);
}

backtest_engine::~backtest_engine()
{
    closeDatabase();  // Ensure database is closed if still open
    delete ui;
}

QStringList backtest_engine::loadTickersFromCSV(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open file:" << filePath;
        return QStringList();
    }
    QString content = file.readAll();
    file.close();

    // Split file into lines (works with both \r\n and \n)
    static const QRegularExpression breakLines("[\r\n]+");
    QStringList lines = content.split(breakLines, Qt::SkipEmptyParts);

    // Trim each line (assuming each line is a ticker)
    for (QString &line : lines) {
        line = line.trimmed();
    }
    return lines;
}

bool backtest_engine::openDatabase(const QString &dbPath)
{
    if (sqlite3_open(dbPath.toStdString().c_str(), &db) != SQLITE_OK) {
        qWarning() << "Could not open database:" << dbPath;
        db = nullptr;
        return false;
    }
    return true;
}

void backtest_engine::closeDatabase()
{
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

void backtest_engine::loadTickersButton_Clicked()
{
    // Using a fixed file path for demonstration; consider using QFileDialog for production
    QString filePath = "C:\\BTE\\build\\Desktop_Qt_6_8_2_MSVC2022_64bit-Debug\\universe.csv";
    QStringList tickerList = loadTickersFromCSV(filePath);
    ui->backtestOutput->setPlainText(tickerList.join(", "));
}

std::unordered_map<std::string, std::vector<Data::Bar>> loadAllData(sqlite3* db) {
    std::unordered_map<std::string, std::vector<Data::Bar>> allData;
    const char* querySQL = "SELECT open, high, low, close, volume, ticker, date FROM Stocks;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, querySQL, -1, &stmt, NULL) != SQLITE_OK) {
        std::cerr << "Error preparing query: " << sqlite3_errmsg(db) << std::endl;
        return allData;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Data::Bar bar;
        bar.open = sqlite3_column_double(stmt, 0);
        bar.high = sqlite3_column_double(stmt, 1);
        bar.low = sqlite3_column_double(stmt, 2);
        bar.close = sqlite3_column_double(stmt, 3);
        bar.volume = sqlite3_column_int(stmt, 4);
        const unsigned char* tickerText = sqlite3_column_text(stmt, 5);
        const unsigned char* dateText = sqlite3_column_text(stmt, 6);
        if (tickerText)
            bar.ticker = reinterpret_cast<const char*>(tickerText);
        if (dateText)
            bar.date = reinterpret_cast<const char*>(dateText);

        // Insert the bar into the map
        allData[bar.ticker].push_back(bar);
    }
    sqlite3_finalize(stmt);
    return allData;
}

void backtest_engine::runBacktest()
{
    QString tickerFilePath = "C:\\BTE\\build\\Desktop_Qt_6_8_2_MSVC2022_64bit-Debug\\universeSmall.csv";
    QString dbPath = "C:\\BTE\\build\\Desktop_Qt_6_8_2_MSVC2022_64bit-Debug\\Universe_OHLCV.db";  // Adjust to your database path

    // Open the database with error checking
    if (!openDatabase(dbPath)) {
        ui->backtestOutput->setPlainText("Failed to open database.");
        return;
    }

    auto allData = loadAllData(db);
    QStringList tickerList = loadTickersFromCSV(tickerFilePath);

    // Initialize aggregate statistics
    AggregateStats aggStats;

    // Process each ticker
    for (const QString &ticker : tickerList) {
        std::string tickerStr = ticker.toStdString();
        auto it = allData.find(tickerStr);
        if (it != allData.end()) {
            Backtest backtest;
            std::vector<TradeRecord> trades = backtest.run(it->second);
            // Update aggregate statistics from each ticker's trades
            for (const auto& trade : trades) {
                aggStats.totalTrades++;
                double profit = trade.sellPrice - trade.buyPrice;
                aggStats.totalProfit += profit;
                if (profit > 0) {
                    aggStats.wins++;
                } else {
                    aggStats.losses++;
                }
            }
        } else {
            qWarning() << "No data found for ticker:" << ticker;
        }
    }

    // Format the aggregate stats into a QString to output to the UI element
    QString resultText = QString("Backtesting completed: \n\nTotal Trades: %1\nTotal Profit: $%2\nWins: %3\nLosses: %4")
                             .arg(aggStats.totalTrades)
                             .arg(aggStats.totalProfit)
                             .arg(aggStats.wins)
                             .arg(aggStats.losses);
    ui->backtestOutput->setPlainText(resultText);

    closeDatabase();  // Close DB connection after processing
}

void backtest_engine::runBacktestButton_Clicked()
{
    runBacktest();
}
