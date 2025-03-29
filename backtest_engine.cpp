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

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QDateTime>


backtest_engine::backtest_engine(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::backtest_engine)
    , db(nullptr)
{
    ui->setupUi(this);

    profitChartView = new QChartView(new QChart(), this);
    profitChartView->setMinimumHeight(200);  // Set a minimum height for visibility

    tradeDetailsTable = new QTableWidget(this);
    tradeDetailsTable->setColumnCount(8);
    tradeDetailsTable->setHorizontalHeaderLabels(QStringList() << "Ticker" << "Buy Price" << "Sell Price" << "Quantity" << "Profit/Loss" << "Buy Date" << "Sell Date" << "Trade Type");
    tradeDetailsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tradeDetailsTable->setSortingEnabled(true);  // Enable column sorting by clicking on headers

    // Integrate the new UI components into the existing layout.
    // Try to get an existing vertical layout; if not found, create one.
    if(ui->verticalLayout) {
        ui->verticalLayout->addWidget(profitChartView);
        ui->verticalLayout_2->addWidget(tradeDetailsTable);
    }


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

std::unordered_map<std::string, std::vector<Data::Bar>> loadAllData(sqlite3* db, int bars) {
    std::unordered_map<std::string, std::vector<Data::Bar>> allData;
    const char* querySQL = "SELECT open, high, low, close, volume, ticker, date FROM ("
                           "  SELECT open, high, low, close, volume, ticker, date, "
                           "         ROW_NUMBER() OVER (PARTITION BY ticker ORDER BY date(date) DESC) as rn "
                           "  FROM Stocks"
                           ") WHERE rn <= ? ORDER BY ticker, date;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, querySQL, -1, &stmt, NULL) != SQLITE_OK) {
        std::cerr << "Error preparing query: " << sqlite3_errmsg(db) << std::endl;
        return allData;
    }

    // Bind the max bars value to the first placeholder.
    if (sqlite3_bind_int(stmt, 1, bars) != SQLITE_OK) {
        std::cerr << "Error binding parameter: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
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

    int maxBars = ui->enterMaxBars->text().toInt();
    if (maxBars <= 0) {
        // Handle invalid input by providing a default value (e.g., 100)
        qWarning() << "Invalid max bars input. Using default value 100.";
        maxBars = 100;
    }

    auto allData = loadAllData(db, maxBars);
    QStringList tickerList = loadTickersFromCSV(tickerFilePath);

    // Initialize aggregate statistics
    AggregateStats aggStats;

    // Container for all trades to be displayed in the chart and table
    std::vector<TradeRecord> allTrades;

    // Process each ticker
    for (const QString &ticker : tickerList) {
        std::string tickerStr = ticker.toStdString();
        auto it = allData.find(tickerStr);

        if (it != allData.end()) {
            Backtest backtest;
            std::vector<TradeRecord> trades = backtest.run(it->second);

            allTrades.insert(allTrades.end(), trades.begin(), trades.end());

            // Update aggregate statistics from each ticker's trades
            for (const auto& trade : trades) {
                aggStats.totalTrades++;
                double profit = (trade.sellPrice - trade.buyPrice) * trade.quantity;
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

    populateProfitLossChart(allTrades);
    populateTradeDetailsTable(allTrades);

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

void backtest_engine::populateProfitLossChart(const std::vector<TradeRecord>& trades)
{
    // Create a new line series for cumulative profit/loss
    QLineSeries *series = new QLineSeries();
    double cumulativeProfit = 0.0;

    // Sort trades by date (assuming trade.date is in "yyyy-MM-dd" format)
    std::vector<TradeRecord> sortedTrades = trades;
    std::sort(sortedTrades.begin(), sortedTrades.end(), [](const TradeRecord &a, const TradeRecord &b) {
        return QString::fromStdString(a.sellDate) < QString::fromStdString(b.sellDate);
    });

    // Add data points to the series
    for (const auto &trade : sortedTrades) {
        cumulativeProfit += (trade.sellPrice - trade.buyPrice) * trade.quantity;
        QDateTime date = QDateTime::fromString(QString::fromStdString(trade.sellDate), "yyyy-MM-dd");
        if (!date.isValid()) {
            // Fallback: use current date if parsing fails
            date = QDateTime::currentDateTime();
        }
        series->append(date.toMSecsSinceEpoch(), cumulativeProfit);
    }

    // Configure the chart
    QChart *chart = profitChartView->chart();
    chart->removeAllSeries();
    chart->addSeries(series);
    chart->setTitle("Cumulative Profit/Loss Over Time");
    chart->legend()->hide();

    // Setup x-axis as a date/time axis
    QDateTimeAxis *axisX = new QDateTimeAxis;
    axisX->setFormat("yyyy-MM-dd");
    axisX->setTitleText("Date");
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    // Setup y-axis as a value axis
    QValueAxis *axisY = new QValueAxis;
    axisY->setTitleText("Cumulative Profit/Loss");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    // Enable antialiasing for smoother lines
    profitChartView->setRenderHint(QPainter::Antialiasing);
}

// --------------------------------------------------------------------
// Helper function to populate the trade details table
// --------------------------------------------------------------------
void backtest_engine::populateTradeDetailsTable(const std::vector<TradeRecord>& trades)
{
    tradeDetailsTable->setRowCount(static_cast<int>(trades.size()));
    int row = 0;
    for (const auto &trade : trades) {
        tradeDetailsTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(trade.ticker)));
        tradeDetailsTable->setItem(row, 1, new QTableWidgetItem(QString::number(trade.buyPrice, 'f', 2)));
        tradeDetailsTable->setItem(row, 2, new QTableWidgetItem(QString::number(trade.sellPrice, 'f', 2)));
        tradeDetailsTable->setItem(row, 3, new QTableWidgetItem(QString::number(trade.quantity, 'f', 2)));
        double profitLoss = (trade.sellPrice - trade.buyPrice) * trade.quantity;
        tradeDetailsTable->setItem(row, 4, new QTableWidgetItem(QString::number(profitLoss, 'f', 2)));
        tradeDetailsTable->setItem(row, 5, new QTableWidgetItem(QString::fromStdString(trade.buyDate)));
        tradeDetailsTable->setItem(row, 6, new QTableWidgetItem(QString::fromStdString(trade.sellDate)));
        tradeDetailsTable->setItem(row, 7, new QTableWidgetItem(QString::fromStdString(trade.info)));
        row++;
    }
}
