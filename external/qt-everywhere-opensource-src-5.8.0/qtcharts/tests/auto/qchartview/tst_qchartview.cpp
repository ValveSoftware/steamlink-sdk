/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Charts module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/QtTest>
#include <QtCharts/qchartview.h>
#include <QtCharts/qlineseries.h>
#include <cmath>
#include <tst_definitions.h>
#include <QtCharts/QValueAxis>

QT_CHARTS_USE_NAMESPACE


Q_DECLARE_METATYPE(QChart*)
Q_DECLARE_METATYPE(QChartView::RubberBands)
Q_DECLARE_METATYPE(Qt::Key)

class tst_QChartView : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private Q_SLOTS:
    void qchartview_data();
    void qchartview();
    void chart_data();
    void chart();
    void rubberBand_data();
    void rubberBand();
    void setChart();

private:
    QChartView* m_view;
};

void tst_QChartView::initTestCase()
{
    //test tracks mouse, give a while to user to relese it
    QTest::qWait(1000);
}

void tst_QChartView::cleanupTestCase()
{
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_QChartView::init()
{
    m_view = new QChartView(newQChartOrQPolarChart());
    m_view->chart()->legend()->setVisible(false);
}

void tst_QChartView::cleanup()
{
    delete m_view;
    m_view =0;
}

void tst_QChartView::qchartview_data()
{

}

void tst_QChartView::qchartview()
{
    QVERIFY(m_view->chart());
    QCOMPARE(m_view->rubberBand(), QChartView::NoRubberBand);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);

    delete(new QChartView());

    QChartView view;
    QVERIFY(view.chart());

}

void tst_QChartView::chart_data()
{

    QTest::addColumn<QChart*>("chart");
    QTest::newRow("qchart") << new QChart();
}

void tst_QChartView::chart()
{
    QFETCH(QChart*, chart);
    QChartView* view = new QChartView(chart);
    QCOMPARE(view->chart(), chart);
    delete view;
}

void tst_QChartView::rubberBand_data()
{
    QTest::addColumn<QChartView::RubberBands>("rubberBand");
    QTest::addColumn<int>("Xcount");
    QTest::addColumn<int>("Ycount");

    QTest::addColumn<QPoint>("min");
    QTest::addColumn<QPoint>("max");

    if (isPolarTest()) {
        QTest::newRow("HorizontalRubberBand") << QChartView::RubberBands(QChartView::HorizontalRubberBand) << 0 << 0 << QPoint(5,5) << QPoint(5,5);
        QTest::newRow("VerticalRubberBand") <<  QChartView::RubberBands(QChartView::VerticalRubberBand) << 0 << 0 << QPoint(5,5) << QPoint(5,5);
        QTest::newRow("RectangleRubberBand") <<  QChartView::RubberBands(QChartView::RectangleRubberBand) << 0 << 0 << QPoint(5,5) << QPoint(5,5);
    } else {
        QTest::newRow("HorizontalRubberBand") << QChartView::RubberBands(QChartView::HorizontalRubberBand) << 0 << 1 << QPoint(5,5) << QPoint(5,5);
        QTest::newRow("VerticalRubberBand") <<  QChartView::RubberBands(QChartView::VerticalRubberBand) << 1 << 0 << QPoint(5,5) << QPoint(5,5);
        QTest::newRow("RectangleRubberBand") <<  QChartView::RubberBands(QChartView::RectangleRubberBand) << 1 << 1 << QPoint(5,5) << QPoint(5,5);
    }
}

void tst_QChartView::rubberBand()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();
    SKIP_IF_FLAKY_MOUSE_MOVE();

    QFETCH(QChartView::RubberBands, rubberBand);
    QFETCH(int, Xcount);
    QFETCH(int, Ycount);
    QFETCH(QPoint, min);
    QFETCH(QPoint, max);

    m_view->setRubberBand(rubberBand);

    QCOMPARE(m_view->rubberBand(), rubberBand);

    QLineSeries* line = new QLineSeries();
    *line << QPointF(0, 0) << QPointF(200, 200);

    m_view->chart()->addSeries(line);
    m_view->chart()->createDefaultAxes();
    m_view->show();
    QTest::qWaitForWindowShown(m_view);

    QRectF plotArea = m_view->chart()->plotArea();
    //this is hack since view does not get events otherwise
    m_view->setMouseTracking(true);

    QAbstractAxis* axisY = m_view->chart()->axisY();
    QSignalSpy spy0(axisY, SIGNAL(rangeChanged(qreal,qreal)));
    QAbstractAxis* axisX = m_view->chart()->axisX();
    QSignalSpy spy1(axisX, SIGNAL(rangeChanged(qreal,qreal)));
    QValueAxis* vaxisX = qobject_cast<QValueAxis*>(axisX);
    QValueAxis* vaxisY = qobject_cast<QValueAxis*>(axisY);
    int minX = vaxisX->min();
    int minY = vaxisY->min();
    int maxX = vaxisX->max();
    int maxY = vaxisY->max();

    // try to ensure focus
    QApplication::setActiveWindow(m_view);
    m_view->setFocus();
    QApplication::processEvents();
    QVERIFY(m_view->isActiveWindow());
    QVERIFY(m_view->hasFocus());

    QTest::mouseMove(m_view->viewport(),  min + plotArea.topLeft().toPoint(), 100);
    QTest::mousePress(m_view->viewport(), Qt::LeftButton, 0,  min + plotArea.topLeft().toPoint(), 100);
    QTest::mouseMove(m_view->viewport(),  plotArea.bottomRight().toPoint() - max, 100);
    QTest::mouseRelease(m_view->viewport(), Qt::LeftButton, 0, plotArea.bottomRight().toPoint() - max, 100);
    QApplication::processEvents();

    TRY_COMPARE(spy0.count(), Xcount);
    TRY_COMPARE(spy1.count(), Ycount);

    //this is hack since view does not get events otherwise
    m_view->setMouseTracking(false);

    QVERIFY(vaxisX->min() >= minX );
    QVERIFY(vaxisX->max() <= maxX );
    QVERIFY(vaxisY->min() >= minY );
    QVERIFY(vaxisY->max() <= maxY );

}

void tst_QChartView::setChart()
{
    QPointer<QChart> oldChart = m_view->chart();
    QLineSeries *series1 = new QLineSeries();
    series1->append(0,0);
    series1->append(1,1);
    oldChart->addSeries(series1);

    QPointer<QChart> newChart = newQChartOrQPolarChart();

    QLineSeries *series2 = new QLineSeries();
    series2->append(0,1);
    series2->append(1,0);
    newChart->addSeries(series2);

    // show current chart
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QTest::qWait(1000);

    // set new chart
    m_view->setChart(newChart);
    QVERIFY(m_view->chart() == newChart);
    QVERIFY(!oldChart.isNull()); // ownership of the oldChart is released and not deleted
    QTest::qWait(1000);

    // delete the view
    delete m_view;
    m_view = 0;
    QVERIFY(newChart.isNull()); // view owns and deletes it
    QVERIFY(!oldChart.isNull()); // not owned by view

    delete oldChart;
}

QTEST_MAIN(tst_QChartView)
#include "tst_qchartview.moc"

