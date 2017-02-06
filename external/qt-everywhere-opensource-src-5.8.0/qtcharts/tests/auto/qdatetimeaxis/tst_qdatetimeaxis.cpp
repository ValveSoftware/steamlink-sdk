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
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>

class tst_QDateTimeAxis : public QObject//: public tst_QAbstractAxis
{
    Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private slots:
    void qdatetimeaxis_data();
    void qdatetimeaxis();

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
    void reverse();

private:
    QDateTimeAxis *m_dateTimeAxisX;
    QDateTimeAxis *m_dateTimeAxisY;
    QLineSeries* m_series;
    QChartView* m_view;
    QChart* m_chart;
};

void tst_QDateTimeAxis::initTestCase()
{
}

void tst_QDateTimeAxis::cleanupTestCase()
{
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_QDateTimeAxis::init()
{
    m_dateTimeAxisX = new QDateTimeAxis();
    m_dateTimeAxisY = new QDateTimeAxis();
    m_series = new QLineSeries();
    *m_series << QPointF(-100, -100) << QPointF(0, 0) << QPointF(100, 100);
    //    tst_QAbstractAxis::init(m_datetimeaxis, m_series);

    m_view = new QChartView;
    m_chart = m_view->chart();
    m_chart->addSeries(m_series);
    m_chart->setAxisY(m_dateTimeAxisY, m_series);
    m_chart->setAxisX(m_dateTimeAxisX, m_series);
}

void tst_QDateTimeAxis::cleanup()
{
    delete m_series;
    delete m_dateTimeAxisX;
    delete m_dateTimeAxisY;
    m_series = 0;
    m_dateTimeAxisX = 0;
    m_dateTimeAxisY = 0;
    delete m_view;
    m_view = 0;
    m_chart = 0;
    //    tst_QAbstractAxis::cleanup();
}

void tst_QDateTimeAxis::qdatetimeaxis_data()
{
}

void tst_QDateTimeAxis::qdatetimeaxis()
{
    //    qabstractaxis();
    QCOMPARE(m_dateTimeAxisX->type(), QAbstractAxis::AxisTypeDateTime);

    m_view->show();
    QTest::qWaitForWindowShown(m_view);

    QVERIFY(m_dateTimeAxisX->max().toMSecsSinceEpoch() != 0);
    QVERIFY(m_dateTimeAxisX->min().toMSecsSinceEpoch() != 0);

    QCOMPARE(m_dateTimeAxisX->isReverse(), false);
    QCOMPARE(m_dateTimeAxisY->isReverse(), false);
}

void tst_QDateTimeAxis::max_raw_data()
{
    QTest::addColumn<QDateTime>("max");
    QTest::addColumn<bool>("valid");
    QDateTime dateTime;
    dateTime.setDate(QDate(2012, 7, 19));
    QTest::newRow("19.7.2012 - Valid") << dateTime << true;
    dateTime.setDate(QDate(2012, 17, 32));
    QTest::newRow("32.17.2012 - Invalid") << dateTime << false;
}

void tst_QDateTimeAxis::max_raw()
{
    QFETCH(QDateTime, max);
    QFETCH(bool, valid);

    QSignalSpy spy0(m_dateTimeAxisX, SIGNAL(maxChanged(QDateTime)));
    QSignalSpy spy1(m_dateTimeAxisX, SIGNAL(minChanged(QDateTime)));
    QSignalSpy spy2(m_dateTimeAxisX, SIGNAL(rangeChanged(QDateTime,QDateTime)));

    m_dateTimeAxisX->setMax(max);

    if (valid) {
        QVERIFY2(m_dateTimeAxisX->max() == max, "Not equal");
        QCOMPARE(spy0.count(), 1);
        QCOMPARE(spy1.count(), 0);
        QCOMPARE(spy2.count(), 1);
    } else {
        QVERIFY2(m_dateTimeAxisX->max() != max, "Date is invalid and should not be set");
        QCOMPARE(spy0.count(), 0);
        QCOMPARE(spy1.count(), 0);
        QCOMPARE(spy2.count(), 0);
    }
}

void tst_QDateTimeAxis::max_data()
{
    max_raw_data();
}

void tst_QDateTimeAxis::max()
{
//    m_chart->setAxisX(m_dateTimeAxisX, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    max_raw();
}

void tst_QDateTimeAxis::max_animation_data()
{
    max_data();
}

void tst_QDateTimeAxis::max_animation()
{
    m_chart->setAnimationOptions(QChart::GridAxisAnimations);
    max();
}

void tst_QDateTimeAxis::min_raw_data()
{
    QTest::addColumn<QDateTime>("min");
    QTest::addColumn<bool>("valid");
    QDateTime dateTime;
    dateTime.setDate(QDate(1908, 1, 11));
    QTest::newRow("11.1.1908 - Valid") << dateTime << true; // negative MSecs from Epoch
    dateTime.setDate(QDate(2012, 17, 32));
    QTest::newRow("32.17.2012 - Invalid") << dateTime << false;
}

void tst_QDateTimeAxis::min_raw()
{
    QFETCH(QDateTime, min);
    QFETCH(bool, valid);

    QSignalSpy spy0(m_dateTimeAxisX, SIGNAL(maxChanged(QDateTime)));
    QSignalSpy spy1(m_dateTimeAxisX, SIGNAL(minChanged(QDateTime)));
    QSignalSpy spy2(m_dateTimeAxisX, SIGNAL(rangeChanged(QDateTime,QDateTime)));

    m_dateTimeAxisX->setMin(min);

    if (valid) {
        QVERIFY2(m_dateTimeAxisX->min() == min, "Not equal");
        QCOMPARE(spy0.count(), 0);
        QCOMPARE(spy1.count(), 1);
        QCOMPARE(spy2.count(), 1);
    } else {
        QVERIFY2(m_dateTimeAxisX->min() != min, "Date is invalid and should not be set");
        QCOMPARE(spy0.count(), 0);
        QCOMPARE(spy1.count(), 0);
        QCOMPARE(spy2.count(), 0);
    }
}

void tst_QDateTimeAxis::min_data()
{
    min_raw_data();
}

void tst_QDateTimeAxis::min()
{
//    m_chart->setAxisX(m_dateTimeAxisX, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    min_raw();
}

void tst_QDateTimeAxis::min_animation_data()
{
    min_data();
}

void tst_QDateTimeAxis::min_animation()
{
    m_chart->setAnimationOptions(QChart::GridAxisAnimations);
    min();
}

void tst_QDateTimeAxis::range_raw_data()
{
    QTest::addColumn<QDateTime>("min");
    QTest::addColumn<bool>("minValid");
    QTest::addColumn<QDateTime>("max");
    QTest::addColumn<bool>("maxValid");

    QDateTime minDateTime;
    QDateTime maxDateTime;
    minDateTime.setDate(QDate(1908, 1, 11));
    maxDateTime.setDate(QDate(1958, 11, 21));
    QTest::newRow("11.1.1908 - min valid, 21.12.1958 - max valid") << minDateTime << true << maxDateTime << true; // negative MSecs from Epoch, min < max

    minDateTime.setDate(QDate(2012, 17, 32));
    QTest::newRow("32.17.2012 - min invalid, 21.12.1958 - max valid") << minDateTime << false << maxDateTime << true;

    maxDateTime.setDate(QDate(2017, 0, 1));
    QTest::newRow("32.17.2012 - min invalid, 1.0.2017 - max invalid") << minDateTime << false << maxDateTime << false;

    minDateTime.setDate(QDate(2012, 1, 1));
    QTest::newRow("1.1.2012 - min valid, 1.0.2017 - max invalid") << minDateTime << true << maxDateTime << false;

    maxDateTime.setDate(QDate(2005, 2, 5));
    QTest::newRow("1.1.2012 - min valid, 5.2.2005 - max valid") << minDateTime << true << maxDateTime << true; // min > max
}

void tst_QDateTimeAxis::range_raw()
{
    QFETCH(QDateTime, min);
    QFETCH(bool, minValid);
    QFETCH(QDateTime, max);
    QFETCH(bool, maxValid);

    QSignalSpy spy0(m_dateTimeAxisX, SIGNAL(maxChanged(QDateTime)));
    QSignalSpy spy1(m_dateTimeAxisX, SIGNAL(minChanged(QDateTime)));
    QSignalSpy spy2(m_dateTimeAxisX, SIGNAL(rangeChanged(QDateTime,QDateTime)));

    m_dateTimeAxisX->setRange(min, max);

    if (minValid && maxValid && min < max) {
        QCOMPARE(spy0.count(), 1);
        QCOMPARE(spy1.count(), 1);
        QCOMPARE(spy2.count(), 1);
    } else {
        QCOMPARE(spy0.count(), 0);
        QCOMPARE(spy1.count(), 0);
        QCOMPARE(spy2.count(), 0);
    }
}

void tst_QDateTimeAxis::range_data()
{
    range_raw_data();
}

void tst_QDateTimeAxis::range()
{
//    m_chart->setAxisX(m_dateTimeAxisX, m_series);
    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    range_raw();
}

void tst_QDateTimeAxis::range_animation_data()
{
    range_data();
}

void tst_QDateTimeAxis::range_animation()
{
    m_chart->setAnimationOptions(QChart::GridAxisAnimations);
    range();
}

void tst_QDateTimeAxis::reverse()
{
    QSignalSpy spy(m_dateTimeAxisX, SIGNAL(reverseChanged(bool)));
    QCOMPARE(m_dateTimeAxisX->isReverse(), false);

    m_dateTimeAxisX->setReverse();
    QCOMPARE(m_dateTimeAxisX->isReverse(), true);
    QCOMPARE(spy.count(), 1);

    m_view->show();
    QTest::qWaitForWindowShown(m_view);
    QCOMPARE(m_dateTimeAxisX->isReverse(), true);
}

QTEST_MAIN(tst_QDateTimeAxis)
#include "tst_qdatetimeaxis.moc"

