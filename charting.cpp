#include "charting.h"
#include <QVBoxLayout>
#include <QPainter>
#include <cfloat>
#include <QLocale>
#include <algorithm>
#include <cmath>

// ----------------------------
// CandlestickChart Implementation
// ----------------------------
CandlestickChart::CandlestickChart(QWidget *parent)
    : QChartView(parent),
    chart(new QChart()),
    priceSeries(new QCandlestickSeries()),
    volumeSeries(nullptr),
    axisX(nullptr),
    axisY(new QValueAxis()),
    axisVolume(nullptr),
    tooltipLabel(new QLabel(this)),
    db(nullptr),
    m_maxBars(120),
    m_verticalLine(nullptr),
    m_horizontalLine(nullptr),
    m_isDragging(false),
    m_rubberBand(new QRubberBand(QRubberBand::Rectangle, this))
{
    // --- Chart Setup ---
    // Enable caching on the chart to cache its rendered image
    chart->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    chart->addSeries(priceSeries);
    chart->setAnimationOptions(QChart::NoAnimation);
    setChart(chart);
    setRenderHint(QPainter::Antialiasing);

    // Enable caching for the QChartView background and smart viewport updates
    setCacheMode(QGraphicsView::CacheBackground);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);

    // Dark background
    QColor darkBackground(16, 16, 16);
    chart->setBackgroundBrush(QBrush(darkBackground));
    chart->setPlotAreaBackgroundBrush(QBrush(darkBackground));
    chart->setDropShadowEnabled(false);

    // --- Price Y-Axis Setup ---
    axisY->setTitleText("Price");
    axisY->setTitleBrush(QBrush(Qt::white));
    axisY->setLabelsColor(Qt::white);
    axisY->setLinePen(QPen(Qt::white));
    axisY->setGridLinePen(QPen(QColor(68, 68, 68)));
    chart->addAxis(axisY, Qt::AlignRight);
    priceSeries->attachAxis(axisY);

    // --- Candlestick Series Appearance ---
    priceSeries->setBodyWidth(0.4);
    priceSeries->setIncreasingColor(QColor(0, 197, 49));
    priceSeries->setDecreasingColor(QColor(255, 95, 95));
    chart->legend()->setVisible(false);

    // --- Tooltip Setup ---
    connect(priceSeries, &QCandlestickSeries::hovered,
            this, &CandlestickChart::handleHovered);
    tooltipLabel->setStyleSheet("background-color: black; color: white; border: 1px solid white; padding: 5px;");
    tooltipLabel->setWindowFlags(Qt::ToolTip);
    tooltipLabel->hide();

    // --- Crosshair Setup ---
    setMouseTracking(true);
    m_verticalLine = new QGraphicsLineItem(chart);
    m_horizontalLine = new QGraphicsLineItem(chart);

    QPen crosshairPen(Qt::white, 1, Qt::DashLine);
    m_verticalLine->setPen(crosshairPen);
    m_horizontalLine->setPen(crosshairPen);

    // Put them in front of other chart items
    m_verticalLine->setZValue(999);
    m_horizontalLine->setZValue(999);

    // Initially hide them (will appear on mouse over).
    m_verticalLine->hide();
    m_horizontalLine->hide();

    // Initialize rubber band for zoom selection (hidden by default)
    m_rubberBand->hide();
}

CandlestickChart::~CandlestickChart() {
    dataCache.clear();
    if (axisVolume) {
        chart->removeAxis(axisVolume);
        delete axisVolume;
    }
}

// Set DB and required functions
void CandlestickChart::setDatabase(sqlite3* dbPtr) {
    db = dbPtr;
}

void CandlestickChart::setMaxBars(int maxBars) {
    m_maxBars = maxBars;
}

bool CandlestickChart::shouldReload(const QString &ticker, bool forceReload)
{
    // Always reload if the ticker is different.
    if (ticker != currentTicker) {
        return true;
    }
    // If the ticker is the same and no forced reload, just reset the zoom.
    if (!dataCache.isEmpty() && !forceReload) {
        chart->zoomReset();
        return false;
    }
    return true;
}

void CandlestickChart::resetChart()
{
    // Clear the candlestick series data
    priceSeries->clear();
    dataCache.clear();

    // Remove & delete old volume series
    if (volumeSeries) {
        chart->removeSeries(volumeSeries);
        delete volumeSeries;
        volumeSeries = nullptr;
    }

    for (QLineSeries* sma : std::as_const(m_smaLines)) {
        chart->removeSeries(sma);
        delete sma;
    }
    m_smaLines.clear();
    clearPriceLevelLines();

    // Remove & delete old axes
    if (axisX) {
        chart->removeAxis(axisX);
        delete axisX;
        axisX = nullptr;
    }
    if (axisVolume) {
        chart->removeAxis(axisVolume);
        delete axisVolume;
        axisVolume = nullptr;
    }
}

// Query and Organize Data
bool CandlestickChart::queryTickerData(const QString &ticker, QVector<CandleData> &outData)
{
    if (!db) {
        qWarning() << "Database not set. Call setDatabase() before loading a ticker.";
        return false;
    }

    // Prepare statement
    const std::string querySQL =
        "SELECT open, high, low, close, volume, date "
        "FROM Stocks "
        "WHERE ticker = ? "
        "ORDER BY date ASC;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, querySQL.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        qWarning() << "Error preparing query:" << sqlite3_errmsg(db);
        return false;
    }

    QByteArray tickerBytes = ticker.toUtf8();
    sqlite3_bind_text(stmt, 1, tickerBytes.constData(), -1, SQLITE_STATIC);

    // Fetch data
    QVector<CandleData> tempData;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        CandleData cd;
        cd.open   = sqlite3_column_double(stmt, 0);
        cd.high   = sqlite3_column_double(stmt, 1);
        cd.low    = sqlite3_column_double(stmt, 2);
        cd.close  = sqlite3_column_double(stmt, 3);
        cd.volume = sqlite3_column_double(stmt, 4);

        const unsigned char* dateChars = sqlite3_column_text(stmt, 5);
        if (!dateChars) continue;
        QString dateString = QString::fromUtf8(reinterpret_cast<const char*>(dateChars));
        QDateTime dt = QDateTime::fromString(dateString, "yyyy-MM-dd");
        if (!dt.isValid()) continue;
        cd.timestamp = dt.toMSecsSinceEpoch();

        tempData.append(cd);
    }
    sqlite3_finalize(stmt);

    outData = tempData;
    return true;
}

void CandlestickChart::updateDataCache(const QVector<CandleData> &allData)
{
    currentTicker.clear();  // We'll set it again after we confirm success.

    // Keep only the most recent m_maxBars entries.
    int total = allData.size();
    if (total <= 0) {
        return; // No data to process
    }

    QVector<CandleData> trimmedData = allData;
    if (total > m_maxBars) {
        trimmedData = allData.mid(total - m_maxBars, m_maxBars);
    }

    dataCache = trimmedData;
}

void CandlestickChart::setupCandleSeries()
{
    // 1) Price range
    double minPrice = DBL_MAX;
    double maxPrice = -DBL_MAX;
    for (const CandleData &cd : std::as_const(dataCache)) {
        minPrice = std::min(minPrice, cd.low);
        maxPrice = std::max(maxPrice, cd.high);
    }
    if (minPrice == DBL_MAX || maxPrice == -DBL_MAX) {
        // No valid data
        return;
    }

    // 2) Create Candlestick Sets
    QVector<QCandlestickSet*> setList;
    for (int i = 0; i < dataCache.size(); ++i) {
        const CandleData &cd = dataCache[i];
        setList.append(new QCandlestickSet(cd.open, cd.high, cd.low, cd.close, i));
    }
    priceSeries->append(setList);

    // 3) Create Category X-Axis
    QStringList categories;
    for (int i = 0; i < dataCache.size(); ++i) {
        categories << QString::number(i);
    }
    axisX = new QBarCategoryAxis();
    axisX->append(categories);
    axisX->setTitleText("Index");
    axisX->setLabelsColor(Qt::white);
    axisX->setLinePen(QPen(Qt::white));
    axisX->setGridLinePen(Qt::NoPen);
    chart->addAxis(axisX, Qt::AlignBottom);
    priceSeries->attachAxis(axisX);

    // 4) Set Price Y-Axis range
    axisY->setRange(minPrice * 0.99, maxPrice * 1.01);
}

void CandlestickChart::setupVolumeSeries()
{
    volumeSeries = new QBarSeries();
    QBarSet *volSet = new QBarSet("Volume");

    QColor volumeColor(0, 0, 255, 128); // Blue with 50% transparency
    volSet->setBrush(volumeColor);
    volSet->setBorderColor(Qt::transparent);

    double maxVolume = 0.0;
    for (int i = 0; i < dataCache.size(); ++i) {
        double scaledVolume = dataCache[i].volume / 4.0; // scale for display
        *volSet << scaledVolume;
        maxVolume = std::max(maxVolume, dataCache[i].volume);
    }
    volumeSeries->append(volSet);
    chart->addSeries(volumeSeries);
    volumeSeries->attachAxis(axisX);

    // Volume Y-axis
    axisVolume = new QValueAxis();
    axisVolume->setRange(0, maxVolume * 1.1);
    axisVolume->setTitleText("Volume");
    axisVolume->setTitleBrush(QBrush(Qt::white));
    axisVolume->setLabelsColor(Qt::white);
    axisVolume->setLinePen(QPen(Qt::white));
    axisVolume->setGridLinePen(QPen(QColor(68, 68, 68)));
    axisVolume->setLabelsVisible(false);
    axisVolume->setLabelFormat("%.0f");
    chart->addAxis(axisVolume, Qt::AlignLeft);
    volumeSeries->attachAxis(axisVolume);
}

void CandlestickChart::setupAxesAndReorderSeries()
{
    // Remove then add them back in the order we want (volume behind candlesticks)
    chart->removeSeries(volumeSeries);
    chart->removeSeries(priceSeries);

    chart->addSeries(volumeSeries);
    chart->addSeries(priceSeries);

    priceSeries->attachAxis(axisX);
    priceSeries->attachAxis(axisY);
    volumeSeries->attachAxis(axisX);
    volumeSeries->attachAxis(axisVolume);
}

// Set up Moving Averages
QVector<double> computeSMA(const QVector<CandleData>& data, int period)
{
    QVector<double> smaValues;
    smaValues.resize(data.size());

    if (data.size() < period || period <= 0) {
        // handle edge cases
        return smaValues;
    }

    double runningSum = 0.0;
    for (int i = 0; i < period; ++i) {
        runningSum += data[i].close;
    }

    // first 'period' values
    for (int i = 0; i < period; ++i) {
        if (i < period - 1) {
            smaValues[i] = 0;
        } else {
            smaValues[i] = runningSum / period;
        }
    }

    // sliding window
    for (int i = period; i < data.size(); ++i) {
        runningSum += data[i].close;
        runningSum -= data[i - period].close;
        smaValues[i] = runningSum / period;
    }

    return smaValues;
}

void CandlestickChart::createMovingAverageLine(int period, const QColor &color)
{
    if (dataCache.isEmpty()) return;

    QVector<double> smaValues = computeSMA(dataCache, period);

    // Create a QLineSeries for SMA
    QLineSeries* smaSeries = new QLineSeries();
    smaSeries->setName(QString("SMA %1").arg(period));

    // Append data: x-values will match candlestick indices
    for (int i = 0; i < dataCache.size(); ++i) {
        if (smaValues[i] > 0.0) {
            smaSeries->append(i, smaValues[i]);
        }
    }

    // Add it to the chart and attach to the axes
    chart->addSeries(smaSeries);
    smaSeries->attachAxis(axisX);
    smaSeries->attachAxis(axisY);

    // Adjust style
    QPen smaPen(color);
    smaPen.setWidth(1);
    smaSeries->setPen(smaPen);

    // Store the SMA line pointer for later removal
    m_smaLines.append(smaSeries);
}

// Main loading function
void CandlestickChart::loadTicker(const QString &ticker, bool forceReload)
{
    // 2) Reset everything
    resetChart();

    // 1) Decide if we need to reload
    if (!shouldReload(ticker, forceReload)) {
        return; // just reset zoom if needed (in shouldReload)
    }

    // 3) Query the new data
    QVector<CandleData> allData;
    if (!queryTickerData(ticker, allData)) {
        qWarning() << "Failed to query data for ticker:" << ticker;
        return;
    }

    // 4) Update the dataCache with the results (trim to max bars, etc.)
    updateDataCache(allData);
    if (dataCache.isEmpty()) {
        qWarning() << "No data returned for ticker:" << ticker;
        return;
    }
    currentTicker = ticker; // we have valid data now

    // 5) Setup candlestick series & axis
    setupCandleSeries();

    // 6) Setup volume series
    setupVolumeSeries();

    // 7) Reorder so volume is behind candlesticks
    setupAxesAndReorderSeries();

    // 8) Optionally add moving averages
    createMovingAverageLine(10, Qt::cyan);
    createMovingAverageLine(20, Qt::red);
    createMovingAverageLine(50, Qt::green);

    // Optionally reset chart zoom
    chart->zoomReset();
}

// --- New Event Handlers for Zooming & Panning ---

// Handle mouse press for drag-to-zoom
void CandlestickChart::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Begin drag selection for zoom
        m_dragStartPos = event->pos();
        m_isDragging = true;
        m_rubberBand->setGeometry(QRect(m_dragStartPos, QSize()));
        m_rubberBand->show();
    }
    QChartView::mousePressEvent(event);
}

// Extend mouse move to update both crosshair and rubber band
void CandlestickChart::mouseMoveEvent(QMouseEvent *event)
{
    // Update rubber band geometry if dragging
    if (m_isDragging) {
        QRect rect(m_dragStartPos, event->pos());
        m_rubberBand->setGeometry(rect.normalized());
    }

    // Existing crosshair code
    QPointF mouseScenePos = mapToScene(event->pos());
    QPointF chartPos = chart->mapFromScene(mouseScenePos);
    QRectF plotArea = chart->plotArea();

    if (plotArea.contains(chartPos)) {
        m_verticalLine->show();
        m_horizontalLine->show();

        QPointF topScene(mouseScenePos.x(), plotArea.top() + chart->scenePos().y());
        QPointF bottomScene(mouseScenePos.x(), plotArea.bottom() + chart->scenePos().y());
        m_verticalLine->setLine(QLineF(topScene, bottomScene));

        QPointF leftScene(plotArea.left() + chart->scenePos().x(), mouseScenePos.y());
        QPointF rightScene(plotArea.right() + chart->scenePos().x(), mouseScenePos.y());
        m_horizontalLine->setLine(QLineF(leftScene, rightScene));
    } else {
        m_verticalLine->hide();
        m_horizontalLine->hide();
    }

    QChartView::mouseMoveEvent(event);
}

// Handle mouse release to apply zoom based on drag selection
void CandlestickChart::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isDragging) {
        m_isDragging = false;
        m_rubberBand->hide();

        QRect selectionRect = m_rubberBand->geometry();
        // Ignore very small selections
        if (selectionRect.width() < 10) {
            QChartView::mouseReleaseEvent(event);
            return;
        }

        // Map the rubber band rectangle (only horizontal) to scene coordinates
        QPointF sceneTopLeft = mapToScene(selectionRect.topLeft());
        QPointF sceneTopRight = mapToScene(QPoint(selectionRect.topRight().x(), selectionRect.topLeft().y()));

        // Use the candlestick series (priceSeries) for mapping, not the axis
        qreal x1 = chart->mapToValue(sceneTopLeft, priceSeries).x();
        qreal x2 = chart->mapToValue(sceneTopRight, priceSeries).x();

        // Clamp the values to the full data range [0, dataCache.size()-1]
        qreal newX1 = std::max(0.0, std::min(x1, x2));
        qreal newX2 = std::min(double(dataCache.size() - 1), std::max(x1, x2));

        // Instead of using chart->zoom() with a QRectF (which is not supported in Qt6),
        // we update the x-axis (a QBarCategoryAxis) range using setRange.
        // The axis expects string labels so we convert our index values to strings.
        int startIndex = static_cast<int>(std::round(newX1));
        int endIndex   = static_cast<int>(std::round(newX2));
        axisX->setRange(QString::number(startIndex), QString::number(endIndex));
    }
    QChartView::mouseReleaseEvent(event);
}

// On double click, reset the zoom to full view
void CandlestickChart::mouseDoubleClickEvent(QMouseEvent *event)
{
    // Reset the x-axis to show the full range (from index 0 to last index)
    if (!dataCache.isEmpty()) {
        axisX->setRange(QString::number(0),
                        QString::number(dataCache.size()-1));
    }
    chart->zoomReset(); // Optional: resets any other zoom state
    QChartView::mouseDoubleClickEvent(event);
}

// Handle mouse wheel event for smooth horizontal scrolling
void CandlestickChart::wheelEvent(QWheelEvent *event)
{
    if (dataCache.isEmpty()) {
        QChartView::wheelEvent(event);
        return;
    }

    // Get the current visible x-axis range using the candlestick series for mapping
    QPointF leftValue = chart->mapToValue(chart->plotArea().topLeft(), priceSeries);
    QPointF rightValue = chart->mapToValue(chart->plotArea().topRight(), priceSeries);
    double currentMin = leftValue.x();
    double currentMax = rightValue.x();
    double visibleRange = currentMax - currentMin;

    // Define a scroll step equal to 10% of the visible range per wheel notch.
    double step = visibleRange * 0.1;
    double offset = (event->angleDelta().y() > 0) ? -step : step;  // scroll direction

    // Clamp so we do not scroll beyond the full data range.
    double fullMin = 0;
    double fullMax = dataCache.size() - 1;
    if (currentMin + offset < fullMin) {
        offset = fullMin - currentMin;
    }
    if (currentMax + offset > fullMax) {
        offset = fullMax - currentMax;
    }

    // Instead of using chart->scroll() directly with pixel offsets,
    // you might consider adjusting the axis range. However, here we compute a pixel offset.
    double pixelOffset = offset * chart->plotArea().width() / visibleRange;
    chart->scroll(pixelOffset, 0);

    event->accept();
}

void CandlestickChart::handleHovered(bool state, QCandlestickSet *set)
{
    if (state && set) {
        int index = static_cast<int>(set->timestamp());
        if (index >= 0 && index < dataCache.size()) {
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(dataCache[index].timestamp);
            QString volumeStr = QLocale(QLocale::English, QLocale::UnitedStates)
                                    .toString(dataCache[index].volume, 'f', 0);
            QString tooltip = QString("Date: %1\nOpen: %2\nHigh: %3\nLow: %4\nClose: %5\nVolume: %6")
                                  .arg(dt.toString("yyyy-MM-dd"))
                                  .arg(set->open())
                                  .arg(set->high())
                                  .arg(set->low())
                                  .arg(set->close())
                                  .arg(volumeStr);
            QPoint globalPos = QCursor::pos();
            tooltipLabel->setText(tooltip);
            tooltipLabel->move(globalPos.x() + 10, globalPos.y() + 10);
            tooltipLabel->show();
        }
    } else {
        tooltipLabel->hide();
    }
}

// (Existing event handlers)
void CandlestickChart::leaveEvent(QEvent *event)
{
    if (m_verticalLine) m_verticalLine->hide();
    if (m_horizontalLine) m_horizontalLine->hide();
    QChartView::leaveEvent(event);
}

void CandlestickChart::resizeEvent(QResizeEvent *event) {
    QChartView::resizeEvent(event);
    // (Optional) reposition tooltip or adjust other elements on resize.
}

void CandlestickChart::drawPriceLevels(double entryPrice, const QColor &lineColor)
{
    QLineSeries *entryPriceSeries = new QLineSeries();
    entryPriceSeries->setColor(lineColor);

    QList<QAbstractAxis *> xAxes = chart->axes(Qt::Horizontal);
    if (xAxes.isEmpty()) {
        qWarning() << "No horizontal axis found.";
        return;
    }

    QBarCategoryAxis *categoryAxis = qobject_cast<QBarCategoryAxis*>(xAxes.first());
    if (!categoryAxis) {
        qWarning() << "Horizontal axis is not a QBarCategoryAxis.";
        return;
    }

    QStringList categories = categoryAxis->categories();
    if (categories.isEmpty()) {
        qWarning() << "No categories in the horizontal axis.";
        return;
    }

    qreal xMin = 0;
    qreal xMax = categories.size() - 1;

    entryPriceSeries->append(xMin, entryPrice);
    entryPriceSeries->append(xMax, entryPrice);

    chart->addSeries(entryPriceSeries);

    QList<QAbstractAxis *> yAxes = chart->axes(Qt::Vertical);
    if (yAxes.isEmpty()) {
        qWarning() << "No vertical axis found.";
        return;
    }
    QAbstractAxis *axisY = yAxes.first();

    entryPriceSeries->attachAxis(categoryAxis);
    entryPriceSeries->attachAxis(axisY);

    m_priceLevelLines.append(entryPriceSeries);
}

void CandlestickChart::clearPriceLevelLines()
{
    for (QLineSeries* series : std::as_const(m_priceLevelLines)) {
        chart->removeSeries(series);
        delete series;
    }
    m_priceLevelLines.clear();
}
