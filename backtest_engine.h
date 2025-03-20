#ifndef BACKTEST_ENGINE_H
#define BACKTEST_ENGINE_H

#include <QWidget>
#include <QStringList>
#include <sqlite3.h>

namespace Ui {
class backtest_engine;
}

class backtest_engine : public QWidget
{
    Q_OBJECT

public:
    explicit backtest_engine(QWidget *parent = nullptr);
    ~backtest_engine();

private slots:
    void loadTickersButton_Clicked();
    void runBacktestButton_Clicked();

private:
    Ui::backtest_engine *ui;
    sqlite3* db; // Database connection

    // Database connection helpers
    bool openDatabase(const QString &dbPath);
    void closeDatabase();

    // Backtesting methods
    void runBacktest();
    QStringList loadTickersFromCSV(const QString &filePath);

    struct AggregateStats {
        int totalTrades = 0;
        double totalProfit = 0.0;
        int wins = 0;
        int losses = 0;
    };
};

#endif // BACKTEST_ENGINE_H
