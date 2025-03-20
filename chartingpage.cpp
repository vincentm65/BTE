#include "chartingpage.h"
#include "charting.h"  // This should define CandlestickChart
#include <QDebug>

ChartingPage::ChartingPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChartingPage)
{
    ui->setupUi(this);

    // Connect QLineEdit signals to the updateChart slot
    connect(ui->enterSymbol, &QLineEdit::editingFinished, this, &ChartingPage::updateChart);
    connect(ui->enterBars, &QLineEdit::editingFinished, this, &ChartingPage::updateChart);
    connect(ui->enterEntryPrice, &QLineEdit::editingFinished, this, &ChartingPage::updateChart);
    connect(ui->enterTakeProfit, &QLineEdit::editingFinished, this, &ChartingPage::updateChart);
    connect(ui->enterStopLoss, &QLineEdit::editingFinished, this, &ChartingPage::updateChart);
    connect(ui->enterRisk, &QLineEdit::editingFinished, this, &ChartingPage::updateChart);
}

ChartingPage::~ChartingPage()
{
    delete ui;
}

CandlestickChart* ChartingPage::getChartWidget() const {
    return ui->chartWidget;
}

void ChartingPage::updateChart() {

    QString newTicker = ui->enterSymbol->text().toUpper();

    // Read user inputs
    QString symbol = ui->enterSymbol->text().toUpper();
    int bars = ui->enterBars->text().toInt();
    int entryPrice = ui->enterEntryPrice->text().toInt();
    int takeProfit = ui->enterTakeProfit->text().toInt();
    int stopLoss = ui->enterStopLoss->text().toInt();
    int risk = ui->enterRisk->text().toInt();

    qDebug() << "Updating chart with symbol:" << symbol << "bars:" << bars;

    if (risk == 0) {
        ui->maxShares->setText("Invalid risk");
    } else {
        ui->maxShares->setText(QString::number(risk/(entryPrice - stopLoss)));
    }

    // Set symbol above chart
    ui->currentTicker->setText("Current symbol: " + symbol);

    // If the ticker has changed, clear the price level fields
    if (newTicker != m_lastTicker) {
        ui->enterEntryPrice->clear();
        ui->enterTakeProfit->clear();
        ui->enterStopLoss->clear();
        m_lastTicker = newTicker;
    }


    // Update the chart: set max bars and reload the ticker
    ui->chartWidget->setMaxBars(bars);
    ui->chartWidget->loadTicker(symbol, true);

    // Need to create lines to represent each price level.
    ui->chartWidget->drawPriceLevels(entryPrice, Qt::gray);
    ui->chartWidget->drawPriceLevels(takeProfit, Qt::green);
    ui->chartWidget->drawPriceLevels(stopLoss, Qt::red);
}
