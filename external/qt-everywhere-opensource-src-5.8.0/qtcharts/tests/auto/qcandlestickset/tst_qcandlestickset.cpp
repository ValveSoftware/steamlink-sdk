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

#include <QtCharts/QCandlestickSet>
#include <QtCore/QDateTime>
#include <QtTest/QtTest>

QT_CHARTS_USE_NAMESPACE

class tst_QCandlestickSet : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private Q_SLOTS:
    void qCandlestickSet_data();
    void qCandlestickSet();
    void timestamp_data();
    void timestamp();
    void open_data();
    void open();
    void high_data();
    void high();
    void low_data();
    void low();
    void close_data();
    void close();
    void brush();
    void pen();

private:
    QCandlestickSet* m_candlestickSet;
};

void tst_QCandlestickSet::initTestCase()
{
}

void tst_QCandlestickSet::cleanupTestCase()
{
    QTest::qWait(1); // Allow final deleteLaters to run
}

void tst_QCandlestickSet::init()
{
    m_candlestickSet = new QCandlestickSet(QDateTime::currentMSecsSinceEpoch());
    m_candlestickSet->setOpen(2345.67);
    m_candlestickSet->setHigh(4567.89);
    m_candlestickSet->setLow(1234.56);
    m_candlestickSet->setClose(3456.78);
}

void tst_QCandlestickSet::cleanup()
{
    delete m_candlestickSet;
    m_candlestickSet = nullptr;
}

void tst_QCandlestickSet::qCandlestickSet_data()
{
    QTest::addColumn<qreal>("timestamp");
    QTest::addColumn<qreal>("expectedTimestamp");

    QTest::newRow("timestamp less than zero") << -1.0 << 0.0;
    QTest::newRow("timestamp equals zero") << 0.0 << 0.0;
    QTest::newRow("timestamp greater than zero") << 1.0 << 1.0;
    QTest::newRow("timestamp rounded down") << 4.321 << 4.0;
    QTest::newRow("timestamp rounded up") << 5.678 << 6.0;
}

void tst_QCandlestickSet::qCandlestickSet()
{
    QFETCH(qreal, timestamp);
    QFETCH(qreal, expectedTimestamp);

    QCandlestickSet candlestickSet(timestamp);
    QCOMPARE(candlestickSet.timestamp(), expectedTimestamp);
}

void tst_QCandlestickSet::timestamp_data()
{
    QTest::addColumn<qreal>("timestamp");
    QTest::addColumn<qreal>("expectedTimestamp");

    QTest::newRow("timestamp less than zero") << -1.0 << 0.0;
    QTest::newRow("timestamp equals zero") << 0.0 << 0.0;
    QTest::newRow("timestamp greater than zero") << 1.0 << 1.0;
    QTest::newRow("timestamp rounded down") << 4.321 << 4.0;
    QTest::newRow("timestamp rounded up") << 5.678 << 6.0;
}

void tst_QCandlestickSet::timestamp()
{
    QFETCH(qreal, timestamp);
    QFETCH(qreal, expectedTimestamp);

    QSignalSpy spy(m_candlestickSet, SIGNAL(timestampChanged()));

    m_candlestickSet->setTimestamp(timestamp);
    QCOMPARE(m_candlestickSet->timestamp(), expectedTimestamp);
    QCOMPARE(spy.count(), 1);

    // Try set same timestamp value
    m_candlestickSet->setTimestamp(expectedTimestamp);
    QCOMPARE(m_candlestickSet->timestamp(), expectedTimestamp);
    QCOMPARE(spy.count(), 1);
}

void tst_QCandlestickSet::open_data()
{
    QTest::addColumn<qreal>("open");

    QTest::newRow("open less than zero") << -1.234;
    QTest::newRow("open equals zero") << 0.0;
    QTest::newRow("open greater than zero") << 1.234;
}

void tst_QCandlestickSet::open()
{
    QFETCH(qreal, open);

    QSignalSpy spy(m_candlestickSet, SIGNAL(openChanged()));

    m_candlestickSet->setOpen(open);
    QCOMPARE(m_candlestickSet->open(), open);
    QCOMPARE(spy.count(), 1);

    // Try set same open value
    m_candlestickSet->setOpen(open);
    QCOMPARE(m_candlestickSet->open(), open);
    QCOMPARE(spy.count(), 1);
}

void tst_QCandlestickSet::high_data()
{
    QTest::addColumn<qreal>("high");

    QTest::newRow("high less than zero") << -1.234;
    QTest::newRow("high equals zero") << 0.0;
    QTest::newRow("high greater than zero") << 1.234;
}

void tst_QCandlestickSet::high()
{
    QFETCH(qreal, high);

    QSignalSpy spy(m_candlestickSet, SIGNAL(highChanged()));

    m_candlestickSet->setHigh(high);
    QCOMPARE(m_candlestickSet->high(), high);
    QCOMPARE(spy.count(), 1);

    // Try set same high value
    m_candlestickSet->setHigh(high);
    QCOMPARE(m_candlestickSet->high(), high);
    QCOMPARE(spy.count(), 1);
}

void tst_QCandlestickSet::low_data()
{
    QTest::addColumn<qreal>("low");

    QTest::newRow("low less than zero") << -1.234;
    QTest::newRow("low equals zero") << 0.0;
    QTest::newRow("low greater than zero") << 1.234;
}

void tst_QCandlestickSet::low()
{
    QFETCH(qreal, low);

    QSignalSpy spy(m_candlestickSet, SIGNAL(lowChanged()));

    m_candlestickSet->setLow(low);
    QCOMPARE(m_candlestickSet->low(), low);
    QCOMPARE(spy.count(), 1);

    // Try set same low value
    m_candlestickSet->setLow(low);
    QCOMPARE(m_candlestickSet->low(), low);
    QCOMPARE(spy.count(), 1);
}

void tst_QCandlestickSet::close_data()
{
    QTest::addColumn<qreal>("close");

    QTest::newRow("close less than zero") << -1.234;
    QTest::newRow("close equals zero") << 0.0;
    QTest::newRow("close greater than zero") << 1.234;
}

void tst_QCandlestickSet::close()
{
    QFETCH(qreal, close);

    QSignalSpy spy(m_candlestickSet, SIGNAL(closeChanged()));

    m_candlestickSet->setClose(close);
    QCOMPARE(m_candlestickSet->close(), close);
    QCOMPARE(spy.count(), 1);

    // Try set same close value
    m_candlestickSet->setClose(close);
    QCOMPARE(m_candlestickSet->close(), close);
    QCOMPARE(spy.count(), 1);
}

void tst_QCandlestickSet::brush()
{
    QSignalSpy spy(m_candlestickSet, SIGNAL(brushChanged()));

    QCOMPARE(m_candlestickSet->brush(), QBrush(Qt::NoBrush));

    m_candlestickSet->setBrush(QBrush(Qt::NoBrush));
    QCOMPARE(m_candlestickSet->brush(), QBrush(Qt::NoBrush));
    QCOMPARE(spy.count(), 0);

    QBrush brush(QColor(128, 128, 128, 128));
    m_candlestickSet->setBrush(brush);
    QCOMPARE(m_candlestickSet->brush(), brush);
    QCOMPARE(spy.count(), 1);

    // Try set same brush
    m_candlestickSet->setBrush(brush);
    QCOMPARE(m_candlestickSet->brush(), brush);
    QCOMPARE(spy.count(), 1);
}

void tst_QCandlestickSet::pen()
{
    QSignalSpy spy(m_candlestickSet, SIGNAL(penChanged()));

    QCOMPARE(m_candlestickSet->pen(), QPen(Qt::NoPen));

    m_candlestickSet->setPen(QPen(Qt::NoPen));
    QCOMPARE(m_candlestickSet->pen(), QPen(Qt::NoPen));
    QCOMPARE(spy.count(), 0);

    QPen pen(QColor(128, 128, 128, 128));
    m_candlestickSet->setPen(pen);
    QCOMPARE(m_candlestickSet->pen(), pen);
    QCOMPARE(spy.count(), 1);

    // Try set same pen
    m_candlestickSet->setPen(pen);
    QCOMPARE(m_candlestickSet->pen(), pen);
    QCOMPARE(spy.count(), 1);
}

QTEST_GUILESS_MAIN(tst_QCandlestickSet)

#include "tst_qcandlestickset.moc"
