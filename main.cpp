#include <QApplication>
#include <QMainWindow>
#include <QHBoxLayout>
#include <QStackedWidget>
#include "mainwindow.h"
#include "menu.h"
#include "backtest.h"
#include "chartingpage.h"
#include "backtest_engine.h"// Our container class

#include <sqlite3.h>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QMainWindow mainWindow;
    QWidget *centralWidget = new QWidget();
    QStackedWidget *stackedWidget = new QStackedWidget();

    // Page 1: MainWindow (or any other widget)
    MainWindow *page1 = new MainWindow();
    stackedWidget->addWidget(page1);

    // Page 2: ChartingPage
    ChartingPage *chartingPage = new ChartingPage();

    // Open the database and configure the chart widget
    sqlite3* db;
    sqlite3_open("Universe_OHLCV.db", &db);
    chartingPage->getChartWidget()->setDatabase(db);
    chartingPage->getChartWidget()->loadTicker("SPY", true);

    stackedWidget->addWidget(chartingPage);

    // Page 3: Backtest Engine
    backtest_engine *backtestEngine = new backtest_engine();
    stackedWidget->addWidget(backtestEngine);

    // For testing purposes, run backtest.
    //Backtest backtest;
    //backtest.run(db, "AAPL");

    // Create a sidebar Menu
    Menu *menu = new Menu();
    menu->setFixedWidth(200);
    QObject::connect(menu, &Menu::navigateToPage, stackedWidget, &QStackedWidget::setCurrentIndex);

    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->addWidget(menu);
    mainLayout->addWidget(stackedWidget);
    mainWindow.setCentralWidget(centralWidget);

    mainWindow.resize(1400, 800);
    mainWindow.show();

    return a.exec();
}
