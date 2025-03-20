#ifndef CHARTINGPAGE_H
#define CHARTINGPAGE_H

#include <QWidget>
#include "ui_chartingpage.h"  // Generated from ChartingPage.ui

// Forward declare your custom chart widget class if needed
class CandlestickChart;

class ChartingPage : public QWidget
{
    Q_OBJECT

public:
    explicit ChartingPage(QWidget *parent = nullptr);
    ~ChartingPage();

    // Public getter to access the promoted chart widget (CandlestickChart)
    CandlestickChart* getChartWidget() const;

private slots:
    void updateChart();  // Slot that reads the QLineEdit inputs and updates the chart

private:
    Ui::ChartingPage *ui;
    QString m_lastTicker;
};

#endif // CHARTINGPAGE_H
