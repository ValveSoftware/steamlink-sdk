/******************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Charts module.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
******************************************************************************/

#include <QtTest/QtTest>
#include <QtGui/QImage>
#include <QtCharts/QChartView>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChartView>
#include <QtCharts/QValueAxis>
#include <tst_definitions.h>

QT_CHARTS_USE_NAMESPACE

class tst_QAreaSeries : public QObject
{
    Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
private slots:
    void areaSeries();
    void dynamicEdgeSeriesChange();

protected:
    QLineSeries *createUpperSeries();
    QLineSeries *createLowerSeries();
    void checkPixels(const QColor &upperColor, const QColor &centerColor, const QColor &lowerColor);

    QChartView *m_view;
    QChart *m_chart;
    QColor m_brushColor;
    QColor m_backgroundColor;
    QValueAxis *m_axisX;
    QValueAxis *m_axisY;
};

void tst_QAreaSeries::initTestCase()
{
    m_brushColor = QColor("#123456");
    m_backgroundColor = QColor("#abcfef");
}

void tst_QAreaSeries::cleanupTestCase()
{
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_QAreaSeries::init()
{
    m_view = new QChartView(newQChartOrQPolarChart());
    m_view->setGeometry(0, 0, 400, 400);
    m_chart = m_view->chart();
    m_chart->setBackgroundBrush(m_backgroundColor);
    m_chart->legend()->setVisible(false);
    m_axisX = new QValueAxis;
    m_axisY = new QValueAxis;
    m_axisX->setRange(0, 4);
    m_axisY->setRange(0, 10);
    // Hide axes so they don't confuse color checks
    m_axisX->setVisible(false);
    m_axisY->setVisible(false);
    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisY, Qt::AlignRight);
}

void tst_QAreaSeries::cleanup()
{
    delete m_view;
    m_view = 0;
    m_chart = 0;
}

void tst_QAreaSeries::areaSeries()
{
    QLineSeries *series0 = createUpperSeries();
    QLineSeries *series1 = createLowerSeries();
    QAreaSeries *series = new QAreaSeries(series0, series1);

    QCOMPARE(series->brush(), QBrush());
    QCOMPARE(series->pen(), QPen());
    QCOMPARE(series->pointsVisible(), false);
    QCOMPARE(series->pointLabelsVisible(), false);
    QCOMPARE(series->pointLabelsFormat(), QLatin1String("@xPoint, @yPoint"));
    QCOMPARE(series->pointLabelsClipping(), true);
    QCOMPARE(series->pointLabelsColor(), QPen().color());
    QCOMPARE(series->upperSeries(), series0);
    QCOMPARE(series->lowerSeries(), series1);
    QCOMPARE(series->color(), QPen().color());
    QCOMPARE(series->borderColor(), QPen().color());

    series->setBrush(QBrush(m_brushColor));
    m_chart->addSeries(series);
    series->attachAxis(m_axisX);
    series->attachAxis(m_axisY);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);

    checkPixels(m_backgroundColor, m_brushColor, m_backgroundColor);
}

void tst_QAreaSeries::dynamicEdgeSeriesChange()
{
    QAreaSeries *series = new QAreaSeries;
    series->setBrush(QBrush(m_brushColor));

    m_chart->addSeries(series);
    series->attachAxis(m_axisX);
    series->attachAxis(m_axisY);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);

    checkPixels(m_backgroundColor, m_backgroundColor, m_backgroundColor);

    QLineSeries *series0 = createUpperSeries();
    series->setUpperSeries(series0);

    QApplication::processEvents();
    checkPixels(m_backgroundColor, m_brushColor, m_brushColor);

    QLineSeries *series1 = createLowerSeries();
    series->setLowerSeries(series1);

    QApplication::processEvents();
    checkPixels(m_backgroundColor, m_brushColor, m_backgroundColor);

    series->setLowerSeries(nullptr);

    QApplication::processEvents();
    checkPixels(m_backgroundColor, m_brushColor, m_brushColor);

    series->setUpperSeries(nullptr);

    QApplication::processEvents();
    checkPixels(m_backgroundColor, m_backgroundColor, m_backgroundColor);
}

QLineSeries *tst_QAreaSeries::createUpperSeries()
{
    QLineSeries *series = new QLineSeries();
    *series << QPointF(0, 10) << QPointF(1, 7) << QPointF(2, 6) << QPointF(3, 7) << QPointF(4, 10);
    return series;
}

QLineSeries *tst_QAreaSeries::createLowerSeries()
{
    QLineSeries *series = new QLineSeries();
    *series << QPointF(0, 0) << QPointF(1, 3) << QPointF(2, 4) << QPointF(3, 3) << QPointF(4, 0);
    return series;
}

void tst_QAreaSeries::checkPixels(const QColor &upperColor,
                                  const QColor &centerColor,
                                  const QColor &lowerColor)
{
    QImage screenGrab = m_view->grab().toImage();
    QCOMPARE(QColor(screenGrab.pixel(200, 50)), upperColor);
    QCOMPARE(QColor(screenGrab.pixel(200, 200)), centerColor);
    QCOMPARE(QColor(screenGrab.pixel(200, 350)), lowerColor);
}

QTEST_MAIN(tst_QAreaSeries)

#include "tst_qareaseries.moc"

