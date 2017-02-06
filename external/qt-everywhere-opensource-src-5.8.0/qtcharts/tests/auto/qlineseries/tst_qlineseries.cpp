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

#include "../qxyseries/tst_qxyseries.h"
#include <QtCharts/QLineSeries>

Q_DECLARE_METATYPE(QList<QPointF>)
Q_DECLARE_METATYPE(QVector<QPointF>)

class tst_QLineSeries : public tst_QXYSeries
{
    Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
private slots:
    void qlineseries_data();
    void qlineseries();
    void pressedSignal();
    void releasedSignal();
    void doubleClickedSignal();
    void insert();
protected:
    void pointsVisible_data();
};

void tst_QLineSeries::initTestCase()
{
}

void tst_QLineSeries::cleanupTestCase()
{
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_QLineSeries::init()
{
    tst_QXYSeries::init();
    m_series = new QLineSeries();
}

void tst_QLineSeries::cleanup()
{
    delete m_series;
    m_series=0;
    tst_QXYSeries::cleanup();
}

void tst_QLineSeries::qlineseries_data()
{

}

void tst_QLineSeries::qlineseries()
{
    QLineSeries series;

    QCOMPARE(series.count(),0);
    QCOMPARE(series.brush(), QBrush());
    QCOMPARE(series.points(), QList<QPointF>());
    QCOMPARE(series.pointsVector(), QVector<QPointF>());
    QCOMPARE(series.pen(), QPen());
    QCOMPARE(series.pointsVisible(), false);
    QCOMPARE(series.pointLabelsVisible(), false);
    QCOMPARE(series.pointLabelsFormat(), QLatin1String("@xPoint, @yPoint"));
    QCOMPARE(series.pointLabelsClipping(), true);

    series.append(QList<QPointF>());
    series.append(0.0,0.0);
    series.append(QPointF());

    series.remove(0.0,0.0);
    series.remove(QPointF());
    series.clear();

    series.replace(QPointF(),QPointF());
    series.replace(0,0,0,0);
    series.setBrush(QBrush());

    series.setPen(QPen());
    series.setPointsVisible(false);

    series.setPointLabelsVisible(false);
    series.setPointLabelsFormat(QString());

    m_chart->addSeries(&series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
}

void tst_QLineSeries::pressedSignal()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();

    QPointF linePoint(4, 12);
    QLineSeries *lineSeries = new QLineSeries();
    lineSeries->append(QPointF(2, 1));
    lineSeries->append(linePoint);
    lineSeries->append(QPointF(6, 12));

    QChartView view;
    view.chart()->legend()->setVisible(false);
    view.chart()->addSeries(lineSeries);
    view.show();
    QTest::qWaitForWindowShown(&view);

    QSignalSpy seriesSpy(lineSeries, SIGNAL(pressed(QPointF)));

    QPointF checkPoint = view.chart()->mapToPosition(linePoint);
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, checkPoint.toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QList<QVariant> seriesSpyArg = seriesSpy.takeFirst();
    // checkPoint is QPointF and for the mouseClick it it's changed to QPoint
    // this causes small distinction in decimals so we round it before comparing
    QPointF signalPoint = qvariant_cast<QPointF>(seriesSpyArg.at(0));
    QCOMPARE(qRound(signalPoint.x()), qRound(linePoint.x()));
    QCOMPARE(qRound(signalPoint.y()), qRound(linePoint.y()));
}

void tst_QLineSeries::releasedSignal()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();

    QPointF linePoint(4, 12);
    QLineSeries *lineSeries = new QLineSeries();
    lineSeries->append(QPointF(2, 20));
    lineSeries->append(linePoint);
    lineSeries->append(QPointF(6, 12));

    QChartView view;
    view.chart()->legend()->setVisible(false);
    view.chart()->addSeries(lineSeries);
    view.show();
    QTest::qWaitForWindowShown(&view);

    QSignalSpy seriesSpy(lineSeries, SIGNAL(released(QPointF)));

    QPointF checkPoint = view.chart()->mapToPosition(linePoint);
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, checkPoint.toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QList<QVariant> seriesSpyArg = seriesSpy.takeFirst();
    // checkPoint is QPointF and for the mouseClick it it's changed to QPoint
    // this causes small distinction in decimals so we round it before comparing
    QPointF signalPoint = qvariant_cast<QPointF>(seriesSpyArg.at(0));
    QCOMPARE(qRound(signalPoint.x()), qRound(linePoint.x()));
    QCOMPARE(qRound(signalPoint.y()), qRound(linePoint.y()));
}

void tst_QLineSeries::insert()
{
    QLineSeries lineSeries;
    QSignalSpy insertSpy(&lineSeries, &QXYSeries::pointAdded);
    lineSeries.insert(0, QPoint(3, 3));
    QCOMPARE(insertSpy.count(), 1);
    QVariantList arguments = insertSpy.takeFirst();
    QCOMPARE(arguments.first().toInt(), 0);
    lineSeries.insert(0, QPoint(1, 1));
    arguments = insertSpy.takeFirst();
    QCOMPARE(arguments.first().toInt(), 0);
    lineSeries.insert(1, QPoint(2, 2));
    arguments = insertSpy.takeFirst();
    QCOMPARE(arguments.first().toInt(), 1);
    lineSeries.insert(42, QPoint(0, 0));
    arguments = insertSpy.takeFirst();
    QCOMPARE(arguments.first().toInt(), 3);
    lineSeries.insert(-42, QPoint(0, 0));
    arguments = insertSpy.takeFirst();
    QCOMPARE(arguments.first().toInt(), 0);
}

void tst_QLineSeries::doubleClickedSignal()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();

    QPointF linePoint(4, 12);
    QLineSeries *lineSeries = new QLineSeries();
    lineSeries->append(QPointF(2, 20));
    lineSeries->append(linePoint);
    lineSeries->append(QPointF(6, 12));

    QChartView view;
    view.chart()->legend()->setVisible(false);
    view.chart()->addSeries(lineSeries);
    view.show();
    QTest::qWaitForWindowShown(&view);

    QSignalSpy seriesSpy(lineSeries, SIGNAL(doubleClicked(QPointF)));

    QPointF checkPoint = view.chart()->mapToPosition(linePoint);
    // mouseClick needed first to save the position
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, checkPoint.toPoint());
    QTest::mouseDClick(view.viewport(), Qt::LeftButton, 0, checkPoint.toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QList<QVariant> seriesSpyArg = seriesSpy.takeFirst();
    // checkPoint is QPointF and for the mouseClick it it's changed to QPoint
    // this causes small distinction in decimals so we round it before comparing
    QPointF signalPoint = qvariant_cast<QPointF>(seriesSpyArg.at(0));
    QCOMPARE(qRound(signalPoint.x()), qRound(linePoint.x()));
    QCOMPARE(qRound(signalPoint.y()), qRound(linePoint.y()));
}

QTEST_MAIN(tst_QLineSeries)

#include "tst_qlineseries.moc"

