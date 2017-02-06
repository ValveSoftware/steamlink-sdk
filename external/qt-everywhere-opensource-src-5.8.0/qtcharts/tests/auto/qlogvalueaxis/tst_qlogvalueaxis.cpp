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
#include <QtCharts/QLogValueAxis>
#include <QtCharts/QLineSeries>
#include <QtCore/QDebug>

class tst_QLogValueAxis: public tst_QAbstractAxis
{
Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private slots:
    void qlogvalueaxis_data();
    void qlogvalueaxis();
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
    void range_raw_data();
    void range_raw();
    void range_data();
    void range();
    void range_animation_data();
    void range_animation();
    void noautoscale_data();
    void noautoscale();
    void autoscale_data();
    void autoscale();
    void zoom();
    void reverse();

private:
    QLogValueAxis* m_logvaluesaxis;
    QLineSeries* m_series;
};

void tst_QLogValueAxis::initTestCase()
{
}

void tst_QLogValueAxis::cleanupTestCase()
{
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_QLogValueAxis::init()
{
    m_logvaluesaxis = new QLogValueAxis();
    m_series = new QLineSeries();
    *m_series << QPointF(1, 1) << QPointF(100, 100);
    tst_QAbstractAxis::init(m_logvaluesaxis,m_series);
    m_chart->addSeries(m_series);
    m_chart->createDefaultAxes();
}

void tst_QLogValueAxis::cleanup()
{
    delete m_series;
    delete m_logvaluesaxis;
    m_series = 0;
    m_logvaluesaxis = 0;
    tst_QAbstractAxis::cleanup();
}

void tst_QLogValueAxis::qlogvalueaxis_data()
{
}

void tst_QLogValueAxis::qlogvalueaxis()
{
    qabstractaxis();

    QCOMPARE(m_logvaluesaxis->max(), (qreal)1);
    QCOMPARE(m_logvaluesaxis->min(), (qreal)1);
    QCOMPARE(m_logvaluesaxis->type(), QAbstractAxis::AxisTypeLogValue);

    m_chart->setAxisX(m_logvaluesaxis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);

    QCOMPARE(m_logvaluesaxis->max(), (qreal)100);
    QCOMPARE(m_logvaluesaxis->min(), (qreal)1);

    QCOMPARE(m_logvaluesaxis->isReverse(), false);
}

void tst_QLogValueAxis::max_raw_data()
{
    QTest::addColumn<qreal>("max");
    QTest::addColumn<qreal>("expected");
    QTest::addColumn<bool>("minChanged");
    QTest::addColumn<bool>("maxChanged");
    QTest::newRow("-1.0") << (qreal)-1.0 << (qreal)1.0 << false << false;
    QTest::newRow("0.0") << (qreal)0.0 << (qreal)1.0 << false << false;
    QTest::newRow("0.5") << (qreal)0.5 << (qreal)0.5 << true << true;
    QTest::newRow("101.0") << (qreal)101.0 << (qreal)101.0 << false << true;
}

void tst_QLogValueAxis::max_raw()
{
    QFETCH(qreal, max);
    QFETCH(qreal, expected);
    QFETCH(bool, minChanged);
    QFETCH(bool, maxChanged);

    // setting the axis in max() changes the max to value other than 1
    // set it back to 1
    m_logvaluesaxis->setMax((qreal)1);

    QSignalSpy spy0(m_logvaluesaxis, SIGNAL(maxChanged(qreal)));
    QSignalSpy spy1(m_logvaluesaxis, SIGNAL(minChanged(qreal)));
    QSignalSpy spy2(m_logvaluesaxis, SIGNAL(rangeChanged(qreal,qreal)));

    m_logvaluesaxis->setMax(max);
    QCOMPARE(m_logvaluesaxis->max(), expected);

    QCOMPARE(spy0.count(), (int)maxChanged);
    QCOMPARE(spy1.count(), (int)minChanged);
    QCOMPARE(spy2.count(), (int)maxChanged);

}

void tst_QLogValueAxis::max_data()
{
    max_raw_data();
}

void tst_QLogValueAxis::max()
{
    m_chart->setAxisX(m_logvaluesaxis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    max_raw();
}

void tst_QLogValueAxis::max_animation_data()
{
    max_data();
}

void tst_QLogValueAxis::max_animation()
{
    m_chart->setAnimationOptions(QChart::GridAxisAnimations);
    max();
}

void tst_QLogValueAxis::min_raw_data()
{
    QTest::addColumn<qreal>("min");
    QTest::addColumn<qreal>("expected");
    QTest::addColumn<bool>("minChanged");
    QTest::addColumn<bool>("maxChanged");
    QTest::newRow("-1.0") << (qreal)-1.0 << (qreal)1.0 << false << false;
    QTest::newRow("0.0") << (qreal)0.0 << (qreal)1.0 << false << false;
    QTest::newRow("0.5") << (qreal)0.5 << (qreal)0.5 << true << false;
    QTest::newRow("101.0") << (qreal)101.0 << (qreal)101.0 << true << true;
}

void tst_QLogValueAxis::min_raw()
{
    QFETCH(qreal, min);
    QFETCH(qreal, expected);
    QFETCH(bool, minChanged);
    QFETCH(bool, maxChanged);

    QSignalSpy spy0(m_logvaluesaxis, SIGNAL(maxChanged(qreal)));
    QSignalSpy spy1(m_logvaluesaxis, SIGNAL(minChanged(qreal)));
    QSignalSpy spy2(m_logvaluesaxis, SIGNAL(rangeChanged(qreal,qreal)));

    m_logvaluesaxis->setMin(min);
    QCOMPARE(m_logvaluesaxis->min(), expected);

    QCOMPARE(spy0.count(), (int)maxChanged);
    QCOMPARE(spy1.count(), (int)minChanged);
    QCOMPARE(spy2.count(), (int)minChanged);
}

void tst_QLogValueAxis::min_data()
{
    min_raw_data();
}

void tst_QLogValueAxis::min()
{
    m_chart->setAxisX(m_logvaluesaxis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    min_raw();
}

void tst_QLogValueAxis::min_animation_data()
{
    min_data();
}

void tst_QLogValueAxis::min_animation()
{
    m_chart->setAnimationOptions(QChart::GridAxisAnimations);
    min();
}

void tst_QLogValueAxis::range_raw_data()
{
    QTest::addColumn<qreal>("min");
    QTest::addColumn<qreal>("max");
    QTest::addColumn<qreal>("expectedMin");
    QTest::addColumn<qreal>("expectedMax");
    QTest::addColumn<bool>("minChanged");
    QTest::addColumn<bool>("maxChanged");
    QTest::newRow("-1.0 - 101.0") << (qreal)-1.0 << (qreal)101.0 << (qreal)1.0 << (qreal)1.0 << false << false;
    QTest::newRow("1.0 - 101.0") << (qreal)1.0 << (qreal)101.0 << (qreal)1.0 << (qreal)101.0 << false << true;
    QTest::newRow("0.1 - 1.0") << (qreal)0.1 << (qreal)1.0 << (qreal)0.1 << (qreal)1.0 << true << false;
    QTest::newRow("25.0 - 75.0") << (qreal)25.0 << (qreal)75.0 << (qreal)25.0 << (qreal)75.0 << true << true;
    QTest::newRow("10.0 - 5.0") << (qreal)10.0 << (qreal)5.0 << (qreal)1.0 << (qreal)1.0 << false << false;
}

void tst_QLogValueAxis::range_raw()
{
    QFETCH(qreal, min);
    QFETCH(qreal, max);
    QFETCH(qreal, expectedMin);
    QFETCH(qreal, expectedMax);
    QFETCH(bool, minChanged);
    QFETCH(bool, maxChanged);

    m_logvaluesaxis->setRange((qreal)1, (qreal)1);

    QSignalSpy spy0(m_logvaluesaxis, SIGNAL(maxChanged(qreal)));
    QSignalSpy spy1(m_logvaluesaxis, SIGNAL(minChanged(qreal)));
    QSignalSpy spy2(m_logvaluesaxis, SIGNAL(rangeChanged(qreal,qreal)));

    m_logvaluesaxis->setRange(min, max);
    QCOMPARE(m_logvaluesaxis->min(), expectedMin);
    QCOMPARE(m_logvaluesaxis->max(), expectedMax);

    QCOMPARE(spy0.count(), (int)maxChanged);
    QCOMPARE(spy1.count(), (int)minChanged);
    QCOMPARE(spy2.count(), (int)(minChanged || maxChanged));
}

void tst_QLogValueAxis::range_data()
{
    range_raw_data();
}

void tst_QLogValueAxis::range()
{
    m_chart->setAxisX(m_logvaluesaxis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    range_raw();
}

void tst_QLogValueAxis::range_animation_data()
{
    range_data();
}

void tst_QLogValueAxis::range_animation()
{
    m_chart->setAnimationOptions(QChart::GridAxisAnimations);
    range();
}

void tst_QLogValueAxis::noautoscale_data()
{
    QTest::addColumn<qreal>("min");
    QTest::addColumn<qreal>("max");
    QTest::newRow("0.1 - 101.0") << (qreal)0.1 << (qreal)101.0;
    QTest::newRow("25.0 - 75.0") << (qreal)25.0 << (qreal)75.0;
}

void tst_QLogValueAxis::noautoscale()
{
    QFETCH(qreal, min);
    QFETCH(qreal, max);

    QSignalSpy spy0(m_logvaluesaxis, SIGNAL(maxChanged(qreal)));
    QSignalSpy spy1(m_logvaluesaxis, SIGNAL(minChanged(qreal)));
    QSignalSpy spy2(m_logvaluesaxis, SIGNAL(rangeChanged(qreal,qreal)));

    m_logvaluesaxis->setRange(min, max);
    QCOMPARE(m_logvaluesaxis->min(), min);
    QCOMPARE(m_logvaluesaxis->max(), max);

    QCOMPARE(spy0.count(), 1);
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(spy2.count(), 1);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(m_logvaluesaxis->min(), min);
    QCOMPARE(m_logvaluesaxis->max(), max);
}

void tst_QLogValueAxis::autoscale_data()
{

}

void tst_QLogValueAxis::autoscale()
{
    QSignalSpy spy0(m_logvaluesaxis, SIGNAL(maxChanged(qreal)));
    QSignalSpy spy1(m_logvaluesaxis, SIGNAL(minChanged(qreal)));
    QSignalSpy spy2(m_logvaluesaxis, SIGNAL(rangeChanged(qreal,qreal)));

    QCOMPARE(m_logvaluesaxis->min(), (qreal)1);
    QCOMPARE(m_logvaluesaxis->max(), (qreal)1);
    m_chart->setAxisX(m_logvaluesaxis, m_series);

    QCOMPARE(spy0.count(), 1);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 1);

    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(m_logvaluesaxis->min(), (qreal)1);
    QCOMPARE(m_logvaluesaxis->max(), (qreal)100);
}

void tst_QLogValueAxis::zoom()
{
    m_chart->setAxisX(m_logvaluesaxis, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    m_logvaluesaxis->setBase(2);
    m_logvaluesaxis->setRange(0.5, 2);

    QCOMPARE(m_logvaluesaxis->min(), (qreal)0.5);
    QCOMPARE(m_logvaluesaxis->max(), (qreal)2.0);

    m_chart->zoomOut();

    QCOMPARE(m_logvaluesaxis->min(), (qreal)0.25);
    QCOMPARE(m_logvaluesaxis->max(), (qreal)4.0);

    m_chart->zoomIn();

    QCOMPARE(m_logvaluesaxis->min(), (qreal)0.5);
    QCOMPARE(m_logvaluesaxis->max(), (qreal)2.0);

    m_logvaluesaxis->setRange(0.5, 1024);
    m_chart->zoom(11.0);

    QCOMPARE(m_logvaluesaxis->min(), (qreal)16.0);
    QCOMPARE(m_logvaluesaxis->max(), (qreal)32.0);

    m_logvaluesaxis->setRange(16, 64);
    m_chart->zoom(1/3.0);

    QCOMPARE(m_logvaluesaxis->min(), (qreal)4);
    QCOMPARE(m_logvaluesaxis->max(), (qreal)256.0);

}

void tst_QLogValueAxis::reverse()
{
    QSignalSpy spy(m_logvaluesaxis, SIGNAL(reverseChanged(bool)));
    QCOMPARE(m_logvaluesaxis->isReverse(), false);

    m_logvaluesaxis->setReverse();
    QCOMPARE(m_logvaluesaxis->isReverse(), true);
    QCOMPARE(spy.count(), 1);

    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(m_logvaluesaxis->isReverse(), true);
}

QTEST_MAIN(tst_QLogValueAxis)
#include "tst_qlogvalueaxis.moc"

