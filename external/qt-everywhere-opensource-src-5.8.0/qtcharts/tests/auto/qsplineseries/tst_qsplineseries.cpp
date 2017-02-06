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
#include <QtCharts/QSplineSeries>

Q_DECLARE_METATYPE(QList<QPointF>)
Q_DECLARE_METATYPE(QVector<QPointF>)

class tst_QSplineSeries : public tst_QXYSeries
{
    Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
private slots:
    void qsplineseries_data();
    void qsplineseries();
    void pressedSignal();
    void releasedSignal();
    void doubleClickedSignal();
protected:
    void pointsVisible_data();
};

void tst_QSplineSeries::initTestCase()
{
}

void tst_QSplineSeries::cleanupTestCase()
{
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_QSplineSeries::init()
{
    tst_QXYSeries::init();
    m_series = new QSplineSeries();
}

void tst_QSplineSeries::cleanup()
{
    delete m_series;
    tst_QXYSeries::cleanup();
}

void tst_QSplineSeries::qsplineseries_data()
{

}

void tst_QSplineSeries::qsplineseries()
{
    QSplineSeries series;

    QCOMPARE(series.count(),0);
    QCOMPARE(series.brush(), QBrush());
    QCOMPARE(series.points(), QList<QPointF>());
    QCOMPARE(series.pointsVector(), QVector<QPointF>());
    QCOMPARE(series.pen(), QPen());
    QCOMPARE(series.pointsVisible(), false);

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

    m_chart->addSeries(&series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
}

void tst_QSplineSeries::pressedSignal()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();

    QPointF splinePoint(4, 12);
    QSplineSeries *splineSeries = new QSplineSeries();
    splineSeries->append(QPointF(2, 1));
    splineSeries->append(splinePoint);
    splineSeries->append(QPointF(6, 12));

    QChartView view;
    view.chart()->legend()->setVisible(false);
    view.chart()->addSeries(splineSeries);
    view.show();
    QTest::qWaitForWindowShown(&view);

    QSignalSpy seriesSpy(splineSeries, SIGNAL(pressed(QPointF)));

    QPointF checkPoint = view.chart()->mapToPosition(splinePoint);
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, checkPoint.toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QList<QVariant> seriesSpyArg = seriesSpy.takeFirst();
    // checkPoint is QPointF and for the mouseClick it it's changed to QPoint
    // this causes small distinction in decimals so we round it before comparing
    QPointF signalPoint = qvariant_cast<QPointF>(seriesSpyArg.at(0));
    QCOMPARE(qRound(signalPoint.x()), qRound(splinePoint.x()));
    QCOMPARE(qRound(signalPoint.y()), qRound(splinePoint.y()));
}

void tst_QSplineSeries::releasedSignal()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();

    QPointF splinePoint(4, 12);
    QSplineSeries *splineSeries = new QSplineSeries();
    splineSeries->append(QPointF(2, 20));
    splineSeries->append(splinePoint);
    splineSeries->append(QPointF(6, 12));

    QChartView view;
    view.chart()->legend()->setVisible(false);
    view.chart()->addSeries(splineSeries);
    view.show();
    QTest::qWaitForWindowShown(&view);

    QSignalSpy seriesSpy(splineSeries, SIGNAL(released(QPointF)));

    QPointF checkPoint = view.chart()->mapToPosition(splinePoint);
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, checkPoint.toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QList<QVariant> seriesSpyArg = seriesSpy.takeFirst();
    // checkPoint is QPointF and for the mouseClick it it's changed to QPoint
    // this causes small distinction in decimals so we round it before comparing
    QPointF signalPoint = qvariant_cast<QPointF>(seriesSpyArg.at(0));
    QCOMPARE(qRound(signalPoint.x()), qRound(splinePoint.x()));
    QCOMPARE(qRound(signalPoint.y()), qRound(splinePoint.y()));
}

void tst_QSplineSeries::doubleClickedSignal()
{
    SKIP_IF_CANNOT_TEST_MOUSE_EVENTS();

    QPointF splinePoint(4, 12);
    QSplineSeries *splineSeries = new QSplineSeries();
    splineSeries->append(QPointF(2, 20));
    splineSeries->append(splinePoint);
    splineSeries->append(QPointF(6, 12));

    QChartView view;
    view.chart()->legend()->setVisible(false);
    view.chart()->addSeries(splineSeries);
    view.show();
    QTest::qWaitForWindowShown(&view);

    QSignalSpy seriesSpy(splineSeries, SIGNAL(doubleClicked(QPointF)));

    QPointF checkPoint = view.chart()->mapToPosition(splinePoint);
    // mouseClick needed first to save the position
    QTest::mouseClick(view.viewport(), Qt::LeftButton, 0, checkPoint.toPoint());
    QTest::mouseDClick(view.viewport(), Qt::LeftButton, 0, checkPoint.toPoint());
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    QCOMPARE(seriesSpy.count(), 1);
    QList<QVariant> seriesSpyArg = seriesSpy.takeFirst();
    // checkPoint is QPointF and for the mouseClick it it's changed to QPoint
    // this causes small distinction in decimals so we round it before comparing
    QPointF signalPoint = qvariant_cast<QPointF>(seriesSpyArg.at(0));
    QCOMPARE(qRound(signalPoint.x()), qRound(splinePoint.x()));
    QCOMPARE(qRound(signalPoint.y()), qRound(splinePoint.y()));
}
QTEST_MAIN(tst_QSplineSeries)

#include "tst_qsplineseries.moc"

