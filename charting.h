#ifndef CHARTING_H
#define CHARTING_H

#include <QtCharts/QChartView>
#include <QtCharts/QValueAxis>
#include <QtCharts/QCandlestickSeries>
#include <QtCharts/QCandlestickSet>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QLineSeries>
#include <QGraphicsLineItem>
#include <QMouseEvent>
#include <QWidget>
#include <QLabel>
#include <QRubberBand>
#include <QVector>
#include <QList>
#include <QTimer>
#include <QDateTime>
#include <sqlite3.h>

// Structure to hold candlestick data (with volume)
struct CandleData {
    double open;
    double high;
    double low;
    double close;
    qint64 timestamp;
    double volume; // volume data
};

class CandlestickChart : public QChartView {
    Q_OBJECT
public:
    explicit CandlestickChart(QWidget *parent = nullptr);
    ~CandlestickChart();

    void setDatabase(sqlite3* dbPtr);
    void setMaxBars(int maxBars);
    void loadTicker(const QString &ticker, bool forceReload = false);
    void drawPriceLevels(double entryPrice, const QColor &lineColor);

public slots:
    void handleHovered(bool state, QCandlestickSet *set);

protected:
    // Override additional mouse events
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    bool shouldReload(const QString &ticker, bool forceReload);
    void resetChart();
    bool queryTickerData(const QString &ticker, QVector<CandleData> &outData);
    void updateDataCache(const QVector<CandleData> &allData);
    void setupCandleSeries();
    void setupVolumeSeries();
    void setupAxesAndReorderSeries();
    void createMovingAverageLine(int period, const QColor &color);
    void clearPriceLevelLines();

private:
    QChart *chart;
    QCandlestickSeries *priceSeries;   // Price (candlestick) series
    QBarSeries *volumeSeries;          // Volume (bar) series
    QBarCategoryAxis *axisX;           // Shared X-axis (categories as indices)
    QValueAxis *axisY;                 // Y-axis for Price (will be on the right)
    QValueAxis *axisVolume;            // Y-axis for Volume (will be on the left)
    QLabel *tooltipLabel;
    QList<QLineSeries*> m_smaLines;
    QList<QLineSeries*> m_priceLevelLines;
    QVector<CandleData> dataCache;
    sqlite3* db;
    QString currentTicker;
    int m_maxBars;

    // Crosshair items
    QGraphicsLineItem *m_verticalLine;
    QGraphicsLineItem *m_horizontalLine;

    // Members for drag-to-zoom and panning
    bool m_isDragging;           // Flag to indicate if a drag is in progress
    QPoint m_dragStartPos;       // The starting point of the drag
    QRubberBand *m_rubberBand;   // Visual rectangle to show selected zoom area

    // New members for incremental updates during panning
    QVector<CandleData> fullData;          // Holds the complete dataset
    int m_windowStartIndex;                // Start index of the current visible window
    int m_windowSize;                      // Size of the current visible window (<= m_maxBars)
    QList<QCandlestickSet*> m_candleSets;  // List of currently displayed candlestick sets

    QPixmap m_cachedPixmap; //Disable anti-alliasing
    bool m_cacheDirty = true;
    QTimer m_cacheTimer;
};

#endif // CHARTING_H
