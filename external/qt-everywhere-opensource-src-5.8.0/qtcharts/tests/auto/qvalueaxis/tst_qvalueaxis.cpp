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

#include "../qabstractaxis/tst_qabstractaxis.h"
#include <QtCharts/QValueAxis>
#include <QtCharts/QLineSeries>

class tst_QValueAxis: public tst_QAbstractAxis
{
Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private slots:
    void qvalueaxis_data();
    void qvalueaxis();
    void max_raw_data();
    void max_raw();
    void max_data();
    void max();
    void max_animation_data();
    void max_animation();
    void min_raw_data();
    void min_raw();
    void min_data();
    void min();
    void min_animation_data();
    void min_animation();
    void applyNiceNumbers_data();
    void applyNiceNumbers();
    void range_raw_data();
    void range_raw();
    void range_data();
    void range();
    void range_animation_data();
    void range_animation();
    void ticksCount_data();
    void ticksCount();
    void noautoscale_data();
    void noautoscale();
    void autoscale_data();
    void autoscale();
    void reverse();

private:
    QValueAxis* m_valuesaxis;
    QLineSeries* m_series;
};

void tst_QValueAxis::initTestCase()
{
}

void tst_QValueAxis::cleanupTestCase()
{
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_QValueAxis::init()
{
    m_valuesaxis = new QValueAxis();
    m_series = new QLineSeries();
    *m_series << QPointF(-100, -100) << QPointF(0, 0) << QPointF(100, 100);
    tst_QAbstractAxis::init(m_valuesaxis,m_series);
    m_chart->addSeries(m_series);
    m_chart->createDefaultAxes();
}

void tst_QValueAxis::cleanup()
{
    delete m_series;
    delete m_valuesaxis;
    m_series = 0;
    m_valuesaxis = 0;
    tst_QAbstractAxis::cleanup();
}

void tst_QValueAxis::qvalueaxis_data()
{
}

void tst_QValueAxis::qvalueaxis()
{
    qabstractaxis();

    QVERIFY(qFuzzyCompare(m_valuesaxis->max(), 0));
    QVERIFY(qFuzzyCompare(m_valuesaxis->min(), 0));
    QCOMPARE(m_valuesaxis->tickCount(), 5);
    QCOMPARE(m_valuesaxis->type(), QAbstractAxis::AxisTypeValue);

    m_chart->setAxisX(m_valuesaxis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);

    QVERIFY(!qFuzzyCompare(m_valuesaxis->max(), 0));
    QVERIFY(!qFuzzyCompare(m_valuesaxis->min(), 0));
    QCOMPARE(m_valuesaxis->tickCount(), 5);

    QCOMPARE(m_valuesaxis->isReverse(), false);
}

void tst_QValueAxis::max_raw_data()
{
    QTest::addColumn<qreal>("max");
    QTest::newRow("1.0") << (qreal)1.0;
    QTest::newRow("50.0") << (qreal)50.0;
    QTest::newRow("101.0") << (qreal)101.0;
}

void tst_QValueAxis::max_raw()
{
    QFETCH(qreal, max);

    QSignalSpy spy0(m_valuesaxis, SIGNAL(maxChanged(qreal)));
    QSignalSpy spy1(m_valuesaxis, SIGNAL(minChanged(qreal)));
    QSignalSpy spy2(m_valuesaxis, SIGNAL(rangeChanged(qreal,qreal)));

    m_valuesaxis->setMax(max);
    QVERIFY2(qFuzzyCompare(m_valuesaxis->max(), max), "Not equal");

    QCOMPARE(spy0.count(), 1);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 1);

}

void tst_QValueAxis::max_data()
{
    max_raw_data();
}

void tst_QValueAxis::max()
{
    m_chart->setAxisX(m_valuesaxis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    max_raw();
}

void tst_QValueAxis::max_animation_data()
{
    max_data();
}

void tst_QValueAxis::max_animation()
{
    m_chart->setAnimationOptions(QChart::GridAxisAnimations);
    max();
}

void tst_QValueAxis::min_raw_data()
{
    QTest::addColumn<qreal>("min");
    QTest::newRow("-1.0") << (qreal)-1.0;
    QTest::newRow("-50.0") << (qreal)-50.0;
    QTest::newRow("-101.0") << (qreal)-101.0;
}

void tst_QValueAxis::min_raw()
{
    QFETCH(qreal, min);

    QSignalSpy spy0(m_valuesaxis, SIGNAL(maxChanged(qreal)));
    QSignalSpy spy1(m_valuesaxis, SIGNAL(minChanged(qreal)));
    QSignalSpy spy2(m_valuesaxis, SIGNAL(rangeChanged(qreal,qreal)));

    m_valuesaxis->setMin(min);
    QVERIFY2(qFuzzyCompare(m_valuesaxis->min(), min), "Not equal");

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(spy2.count(), 1);
}

void tst_QValueAxis::min_data()
{
    min_raw_data();
}

void tst_QValueAxis::min()
{
    m_chart->setAxisX(m_valuesaxis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    min_raw();
}

void tst_QValueAxis::min_animation_data()
{
    min_data();
}

void tst_QValueAxis::min_animation()
{
    m_chart->setAnimationOptions(QChart::GridAxisAnimations);
    min();
}

void tst_QValueAxis::applyNiceNumbers_data()
{
    QTest::addColumn<bool>("niceNumbersEnabled");
    QTest::addColumn<qreal>("min");
    QTest::addColumn<qreal>("max");
    QTest::addColumn<int>("ticks");
    QTest::addColumn<qreal>("expectedMin");
    QTest::addColumn<qreal>("expectedMax");
    QTest::addColumn<int>("expectedTicks");
    QTest::newRow("true 0.1 , 99.0 , 5") << true << (qreal)0.1 << (qreal)99.0 << 5 << (qreal)0.0 << (qreal)100.0 << 6;
    QTest::newRow("true 1 , 10.0 , 5") << true << (qreal)1.0 << (qreal)10.0 << 5 << (qreal)0.0 << (qreal)10.0 << 6;
    QTest::newRow("true 0.1 , 6.6 , 5") << true << (qreal)0.1 << (qreal)6.6 << 5 << (qreal)0.0 << (qreal)8.0 << 5;
    QTest::newRow("false 0.1 , 6.6 , 5") << false << (qreal)0.1 << (qreal)6.6 << 5 << (qreal)0.1 << (qreal)6.6 << 5;
    QTest::newRow("true 0.1, 99, 5") << true << (qreal)0.1 << (qreal)99.0 << 5 << (qreal)0.0 << (qreal)100.0 << 6;
    QTest::newRow("true 5, 93.5 , 5") << true << (qreal)5.0 << (qreal)93.5 << 5 << (qreal)0.0 << (qreal)100.0 << 6;
}

void tst_QValueAxis::applyNiceNumbers()
{
    QFETCH(bool, niceNumbersEnabled);
    QFETCH(qreal, min);
    QFETCH(qreal, max);
    QFETCH(int, ticks);
    QFETCH(qreal, expectedMin);
    QFETCH(qreal, expectedMax);
    QFETCH(int, expectedTicks);

    m_valuesaxis->setRange(min, max);
    m_valuesaxis->setTickCount(ticks);

    QVERIFY2(qFuzzyCompare(m_valuesaxis->min(), min), "Min not equal");
    QVERIFY2(qFuzzyCompare(m_valuesaxis->max(), max), "Max not equal");

    QSignalSpy spy0(m_valuesaxis, SIGNAL(maxChanged(qreal)));
    QSignalSpy spy1(m_valuesaxis, SIGNAL(minChanged(qreal)));
    QSignalSpy spy2(m_valuesaxis, SIGNAL(rangeChanged(qreal,qreal)));

    if(niceNumbersEnabled) m_valuesaxis->applyNiceNumbers();

    if(!qFuzzyCompare(expectedMin, min))
        QCOMPARE(spy1.count(), 1);
    if(!qFuzzyCompare(expectedMax, max))
        QCOMPARE(spy0.count(), 1);
    if((!qFuzzyCompare(expectedMin, min)) || (!qFuzzyCompare(expectedMax, max)))
        QCOMPARE(spy2.count(), 1);

    QVERIFY2(qFuzzyCompare(m_valuesaxis->min(), expectedMin), "Min not equal");
    QVERIFY2(qFuzzyCompare(m_valuesaxis->max(), expectedMax), "Max not equal");
    QCOMPARE(m_valuesaxis->tickCount(), expectedTicks);

}

void tst_QValueAxis::range_raw_data()
{
    QTest::addColumn<qreal>("min");
    QTest::addColumn<qreal>("max");
    QTest::newRow("1.0 - 101.0") << (qreal)-1.0 << (qreal)101.0;
    QTest::newRow("25.0 - 75.0") << (qreal)25.0 << (qreal)75.0;
    QTest::newRow("101.0") << (qreal)40.0 << (qreal)60.0;
}

void tst_QValueAxis::range_raw()
{
    QFETCH(qreal, min);
    QFETCH(qreal, max);

    QSignalSpy spy0(m_valuesaxis, SIGNAL(maxChanged(qreal)));
    QSignalSpy spy1(m_valuesaxis, SIGNAL(minChanged(qreal)));
    QSignalSpy spy2(m_valuesaxis, SIGNAL(rangeChanged(qreal,qreal)));

    m_valuesaxis->setRange(min, max);
    QVERIFY2(qFuzzyCompare(m_valuesaxis->min(), min), "Min not equal");
    QVERIFY2(qFuzzyCompare(m_valuesaxis->max(), max), "Max not equal");

    QCOMPARE(spy0.count(), 1);
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(spy2.count(), 1);
}

void tst_QValueAxis::range_data()
{
    range_raw_data();
}

void tst_QValueAxis::range()
{
    m_chart->setAxisX(m_valuesaxis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    range_raw();
}

void tst_QValueAxis::range_animation_data()
{
    range_data();
}

void tst_QValueAxis::range_animation()
{
    m_chart->setAnimationOptions(QChart::GridAxisAnimations);
    range();
}

void tst_QValueAxis::ticksCount_data()
{
    QTest::addColumn<int>("ticksCount");
    QTest::addColumn<int>("expectedCount");
    QTest::newRow("0") << 2;
    QTest::newRow("1") << 2;
    QTest::newRow("2") << 2;
    QTest::newRow("3") << 3;
    QTest::newRow("-1") << 2;
}

void tst_QValueAxis::ticksCount()
{
    QFETCH(int, ticksCount);

    QSignalSpy spy0(m_valuesaxis, SIGNAL(maxChanged(qreal)));
    QSignalSpy spy1(m_valuesaxis, SIGNAL(minChanged(qreal)));
    QSignalSpy spy2(m_valuesaxis, SIGNAL(rangeChanged(qreal,qreal)));

    m_valuesaxis->setTickCount(ticksCount);
    QCOMPARE(m_valuesaxis->tickCount(), ticksCount);

    QCOMPARE(spy0.count(), 0);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);

    m_chart->setAxisX(m_valuesaxis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);

    QCOMPARE(m_valuesaxis->tickCount(), ticksCount);
}

void tst_QValueAxis::noautoscale_data()
{
    QTest::addColumn<qreal>("min");
    QTest::addColumn<qreal>("max");
    QTest::newRow("1.0 - 101.0") << (qreal)-1.0 << (qreal)101.0;
    QTest::newRow("25.0 - 75.0") << (qreal)25.0 << (qreal)75.0;
    QTest::newRow("101.0") << (qreal)40.0 << (qreal)60.0;
}

void tst_QValueAxis::noautoscale()
{
    QFETCH(qreal, min);
    QFETCH(qreal, max);

    QSignalSpy spy0(m_valuesaxis, SIGNAL(maxChanged(qreal)));
    QSignalSpy spy1(m_valuesaxis, SIGNAL(minChanged(qreal)));
    QSignalSpy spy2(m_valuesaxis, SIGNAL(rangeChanged(qreal,qreal)));

    m_valuesaxis->setRange(min, max);
    QVERIFY2(qFuzzyCompare(m_valuesaxis->min(), min), "Min not equal");
    QVERIFY2(qFuzzyCompare(m_valuesaxis->max(), max), "Max not equal");

    QCOMPARE(spy0.count(), 1);
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(spy2.count(), 1);

    m_chart->setAxisX(m_valuesaxis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QVERIFY2(qFuzzyCompare(m_valuesaxis->min(), min), "Min not equal");
    QVERIFY2(qFuzzyCompare(m_valuesaxis->max(), max), "Max not equal");
}

void tst_QValueAxis::autoscale_data()
{

}

void tst_QValueAxis::autoscale()
{
    QSignalSpy spy0(m_valuesaxis, SIGNAL(maxChanged(qreal)));
    QSignalSpy spy1(m_valuesaxis, SIGNAL(minChanged(qreal)));
    QSignalSpy spy2(m_valuesaxis, SIGNAL(rangeChanged(qreal,qreal)));

    QVERIFY2(qFuzzyCompare(m_valuesaxis->min(), 0), "Min not equal");
    QVERIFY2(qFuzzyCompare(m_valuesaxis->max(), 0), "Max not equal");
    m_chart->setAxisX(m_valuesaxis, m_series);

    QCOMPARE(spy0.count(), 1);
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(spy2.count(), 1);

    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QVERIFY2(qFuzzyCompare(m_valuesaxis->min(), -100), "Min not equal");
    QVERIFY2(qFuzzyCompare(m_valuesaxis->max(), 100), "Max not equal");
}

void tst_QValueAxis::reverse()
{
    QSignalSpy spy(m_valuesaxis, SIGNAL(reverseChanged(bool)));
    QCOMPARE(m_valuesaxis->isReverse(), false);

    m_valuesaxis->setReverse();
    QCOMPARE(m_valuesaxis->isReverse(), true);

    m_chart->setAxisX(m_valuesaxis, m_series);
    QCOMPARE(spy.count(), 1);

    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(m_valuesaxis->isReverse(), true);
}

QTEST_MAIN(tst_QValueAxis)
#include "tst_qvalueaxis.moc"

