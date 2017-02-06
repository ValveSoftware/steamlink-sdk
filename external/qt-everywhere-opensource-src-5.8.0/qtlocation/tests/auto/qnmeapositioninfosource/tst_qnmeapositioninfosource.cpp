/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd.
** Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

//TESTED_COMPONENT=src/location

#include "tst_qnmeapositioninfosource.h"

#include <QtCore/QtNumeric>

#ifdef Q_OS_WIN

// Windows seems to require longer timeouts and step length
// We override the standard QTestCase related macros

#ifdef QTRY_COMPARE_WITH_TIMEOUT
#undef  QTRY_COMPARE_WITH_TIMEOUT
#endif
#define QTRY_COMPARE_WITH_TIMEOUT(__expr, __expected, __timeout) \
do { \
    const int __step = 100; \
    const int __timeoutValue = __timeout; \
    if ((__expr) != (__expected)) { \
        QTest::qWait(0); \
    } \
    for (int __i = 0; __i < __timeoutValue && ((__expr) != (__expected)); __i+=__step) { \
        QTest::qWait(__step); \
    } \
    QCOMPARE(__expr, __expected); \
} while (0)

#ifdef QTRY_COMPARE
#undef  QTRY_COMPARE
#endif
#define QTRY_COMPARE(__expr, __expected) QTRY_COMPARE_WITH_TIMEOUT(__expr, __expected, 10000)

#endif

tst_QNmeaPositionInfoSource::tst_QNmeaPositionInfoSource(QNmeaPositionInfoSource::UpdateMode mode, QObject *parent)
    : QObject(parent),
      m_mode(mode)
{
}

void tst_QNmeaPositionInfoSource::initTestCase()
{
    qRegisterMetaType<QGeoPositionInfo>();
    qRegisterMetaType<QNmeaPositionInfoSource::UpdateMode>();
}

void tst_QNmeaPositionInfoSource::constructor()
{
    QObject o;
    QNmeaPositionInfoSource source(m_mode, &o);
    QCOMPARE(source.updateMode(), m_mode);
    QCOMPARE(source.parent(), &o);
}

void tst_QNmeaPositionInfoSource::supportedPositioningMethods()
{
    QNmeaPositionInfoSource source(m_mode);
    QCOMPARE(source.supportedPositioningMethods(), QNmeaPositionInfoSource::SatellitePositioningMethods);
}

void tst_QNmeaPositionInfoSource::minimumUpdateInterval()
{
    QNmeaPositionInfoSource source(m_mode);
    QCOMPARE(source.minimumUpdateInterval(), 100);
}

void tst_QNmeaPositionInfoSource::userEquivalentRangeError()
{
    QNmeaPositionInfoSource source(m_mode);
    QVERIFY(qIsNaN(source.userEquivalentRangeError()));
    source.setUserEquivalentRangeError(5.1);
    QVERIFY(qFuzzyCompare(source.userEquivalentRangeError(), 5.1));
}

void tst_QNmeaPositionInfoSource::setUpdateInterval_delayedUpdate()
{
    // If an update interval is set, and an update is not available at a
    // particular interval, the source should emit the next update as soon
    // as it becomes available

    QNmeaPositionInfoSource source(m_mode);
    QNmeaPositionInfoSourceProxyFactory factory;
    QNmeaPositionInfoSourceProxy *proxy = static_cast<QNmeaPositionInfoSourceProxy*>(factory.createProxy(&source));

    QSignalSpy spyUpdate(proxy->source(), SIGNAL(positionUpdated(QGeoPositionInfo)));
    proxy->source()->setUpdateInterval(500);
    proxy->source()->startUpdates();

    QTest::qWait(600);
    QDateTime now = QDateTime::currentDateTime();
    proxy->feedUpdate(now);
    QTRY_COMPARE(spyUpdate.count(), 1);

    // should have gotten the update immediately, and not have needed to
    // wait until the next interval
    QVERIFY(now.time().msecsTo(QDateTime::currentDateTime().time()) < 200);
}

void tst_QNmeaPositionInfoSource::lastKnownPosition()
{
    QNmeaPositionInfoSource source(m_mode);
    QNmeaPositionInfoSourceProxyFactory factory;
    QNmeaPositionInfoSourceProxy *proxy = static_cast<QNmeaPositionInfoSourceProxy*>(factory.createProxy(&source));

    QCOMPARE(proxy->source()->lastKnownPosition(), QGeoPositionInfo());

    // source may need requestUpdate() or startUpdates() to be called to
    // trigger reading of data channel
    QSignalSpy spyTimeout(proxy->source(), SIGNAL(updateTimeout()));
    proxy->source()->requestUpdate(proxy->source()->minimumUpdateInterval());
    QTRY_COMPARE(spyTimeout.count(), 1);

    // If an update is received and startUpdates() or requestUpdate() hasn't
    // been called, it should still be available through lastKnownPosition()
    QDateTime dt = QDateTime::currentDateTime().toUTC();
    proxy->feedUpdate(dt);
    QTRY_COMPARE(proxy->source()->lastKnownPosition().timestamp(), dt);

    QList<QDateTime> dateTimes = createDateTimes(5);
    for (int i=0; i<dateTimes.count(); i++) {
        proxy->source()->requestUpdate();
        proxy->feedUpdate(dateTimes[i]);
        QTRY_COMPARE(proxy->source()->lastKnownPosition().timestamp(), dateTimes[i]);
    }

    proxy->source()->startUpdates();
    dateTimes = createDateTimes(5);
    for (int i=0; i<dateTimes.count(); i++) {
        proxy->feedUpdate(dateTimes[i]);
        QTRY_COMPARE(proxy->source()->lastKnownPosition().timestamp(), dateTimes[i]);
    }
}

void tst_QNmeaPositionInfoSource::beginWithBufferedData()
{
    // In SimulationMode, data stored in the QIODevice is read when
    // startUpdates() or requestUpdate() is called.
    // In RealTimeMode, all existing data in the QIODevice is ignored -
    // only new data will be read.

    QFETCH(QList<QDateTime>, dateTimes);
    QFETCH(UpdateTriggerMethod, trigger);

    QByteArray bytes;
    for (int i=0; i<dateTimes.count(); i++)
        bytes += QLocationTestUtils::createRmcSentence(dateTimes[i]).toLatin1();
    QBuffer buffer;
    buffer.setData(bytes);

    QNmeaPositionInfoSource source(m_mode);
    QSignalSpy spy(&source, SIGNAL(positionUpdated(QGeoPositionInfo)));
    source.setDevice(&buffer);

    if (trigger == StartUpdatesMethod)
        source.startUpdates();
    else if (trigger == RequestUpdatesMethod)
        source.requestUpdate();

    if (m_mode == QNmeaPositionInfoSource::RealTimeMode) {
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 0, 300);
    } else {
        if (trigger == StartUpdatesMethod) {
            QTRY_COMPARE(spy.count(), dateTimes.count());
            for (int i=0; i<dateTimes.count(); i++)
                QCOMPARE(spy.at(i).at(0).value<QGeoPositionInfo>().timestamp(), dateTimes[i]);
        } else if (trigger == RequestUpdatesMethod) {
            QTRY_COMPARE(spy.count(), 1);
            QCOMPARE(spy.at(0).at(0).value<QGeoPositionInfo>().timestamp(), dateTimes.first());
        }
    }
}

void tst_QNmeaPositionInfoSource::beginWithBufferedData_data()
{
    QTest::addColumn<QList<QDateTime> >("dateTimes");
    QTest::addColumn<UpdateTriggerMethod>("trigger");

    QList<QDateTime> dateTimes;
    dateTimes << QDateTime::currentDateTime().toUTC();

    QTest::newRow("startUpdates(), 1 update in buffer") << dateTimes << StartUpdatesMethod;
    QTest::newRow("requestUpdate(), 1 update in buffer") << dateTimes << RequestUpdatesMethod;

    for (int i=1; i<3; i++)
        dateTimes << dateTimes[0].addDays(i);
    QTest::newRow("startUpdates(), multiple updates in buffer") << dateTimes << StartUpdatesMethod;
    QTest::newRow("requestUpdate(), multiple updates in buffer") << dateTimes << RequestUpdatesMethod;
}

void tst_QNmeaPositionInfoSource::startUpdates()
{
    QFETCH(QList<QDateTime>, dateTimes);

    QNmeaPositionInfoSource source(m_mode);
    QNmeaPositionInfoSourceProxyFactory factory;
    QNmeaPositionInfoSourceProxy *proxy = static_cast<QNmeaPositionInfoSourceProxy*>(factory.createProxy(&source));

    QSignalSpy spyUpdate(proxy->source(), SIGNAL(positionUpdated(QGeoPositionInfo)));
    proxy->source()->startUpdates();

    for (int i=0; i<dateTimes.count(); i++)
        proxy->feedUpdate(dateTimes[i]);
    QTRY_COMPARE(spyUpdate.count(), dateTimes.count());
}

void tst_QNmeaPositionInfoSource::startUpdates_data()
{
    QTest::addColumn<QList<QDateTime> >("dateTimes");

    QTest::newRow("1 update") << createDateTimes(1);
    QTest::newRow("2 updates") << createDateTimes(2);
    QTest::newRow("10 updates") << createDateTimes(10);
}

void tst_QNmeaPositionInfoSource::startUpdates_withTimeout()
{
    QNmeaPositionInfoSource source(m_mode);
    QNmeaPositionInfoSourceProxyFactory factory;
    QNmeaPositionInfoSourceProxy *proxy = static_cast<QNmeaPositionInfoSourceProxy*>(factory.createProxy(&source));

    QSignalSpy spyUpdate(proxy->source(), SIGNAL(positionUpdated(QGeoPositionInfo)));
    QSignalSpy spyTimeout(proxy->source(), SIGNAL(updateTimeout()));

    proxy->source()->setUpdateInterval(1000);
    proxy->source()->startUpdates();

    QDateTime dt = QDateTime::currentDateTime().toUTC();

    if (m_mode == QNmeaPositionInfoSource::SimulationMode) {
        // the first sentence primes the simulation
        proxy->feedBytes(QLocationTestUtils::createRmcSentence(dt).toLatin1());
        proxy->feedBytes(QLocationTestUtils::createRmcSentence(dt.addMSecs(10)).toLatin1());
        proxy->feedBytes(QLocationTestUtils::createRmcSentence(dt.addMSecs(1100)).toLatin1());
        proxy->feedBytes(QLocationTestUtils::createRmcSentence(dt.addMSecs(2200)).toLatin1());
        proxy->feedBytes(QLocationTestUtils::createRmcSentence(dt.addSecs(9)).toLatin1());

        QTime t;
        t.start();

        for (int j = 1; j < 4; ++j) {
            QTRY_COMPARE(spyUpdate.count(), j);
            QCOMPARE(spyTimeout.count(), 0);
            int time = t.elapsed();
            QVERIFY((time > j*1000 - 300) && (time < j*1000 + 300));
        }

        spyUpdate.clear();

        QTRY_VERIFY_WITH_TIMEOUT((spyUpdate.count() == 0) && (spyTimeout.count() == 1), 7500);
        spyTimeout.clear();

        QTRY_VERIFY_WITH_TIMEOUT((spyUpdate.count() == 1) && (spyTimeout.count() == 0), 7500);

    } else {
        // dt + 900
        QTRY_VERIFY(spyUpdate.count() == 0 && spyTimeout.count() == 0);

        proxy->feedBytes(QLocationTestUtils::createRmcSentence(dt.addSecs(1)).toLatin1());
        // dt + 1200
        QTRY_VERIFY(spyUpdate.count() == 1 && spyTimeout.count() == 0);
        spyUpdate.clear();

        // dt + 1900
        QTRY_VERIFY(spyUpdate.count() == 0 && spyTimeout.count() == 0);
        proxy->feedBytes(QLocationTestUtils::createRmcSentence(dt.addSecs(2)).toLatin1());

        // dt + 2200
        QTRY_VERIFY(spyUpdate.count() == 1 && spyTimeout.count() == 0);
        spyUpdate.clear();

        // dt + 2900
        QTRY_VERIFY(spyUpdate.count() == 0 && spyTimeout.count() == 0);
        proxy->feedBytes(QLocationTestUtils::createRmcSentence(dt.addSecs(3)).toLatin1());

        // dt + 3200
        QTRY_VERIFY(spyUpdate.count() == 1 && spyTimeout.count() == 0);
        spyUpdate.clear();

        // dt + 6900
        QTRY_VERIFY(spyUpdate.count() == 0 && spyTimeout.count() == 1);
        spyTimeout.clear();
        proxy->feedBytes(QLocationTestUtils::createRmcSentence(dt.addSecs(7)).toLatin1());

        // dt + 7200
        QTRY_VERIFY(spyUpdate.count() == 1 && spyTimeout.count() == 0);
        spyUpdate.clear();
    }
}

void tst_QNmeaPositionInfoSource::startUpdates_expectLatestUpdateOnly()
{
    // If startUpdates() is called and an interval has been set, if multiple'
    // updates are in the buffer, only the latest update should be emitted

    QNmeaPositionInfoSource source(m_mode);
    QNmeaPositionInfoSourceProxyFactory factory;
    QNmeaPositionInfoSourceProxy *proxy = static_cast<QNmeaPositionInfoSourceProxy*>(factory.createProxy(&source));

    QSignalSpy spyUpdate(proxy->source(), SIGNAL(positionUpdated(QGeoPositionInfo)));
    proxy->source()->setUpdateInterval(500);
    proxy->source()->startUpdates();

    QList<QDateTime> dateTimes = createDateTimes(3);
    for (int i=0; i<dateTimes.count(); i++)
        proxy->feedUpdate(dateTimes[i]);

    QTRY_COMPARE(spyUpdate.count(), 1);
    QCOMPARE(spyUpdate[0][0].value<QGeoPositionInfo>().timestamp(), dateTimes.last());
}

void tst_QNmeaPositionInfoSource::startUpdates_waitForValidDateTime()
{
    // Tests that the class does not emit an update until it receives a
    // sentences with a valid date *and* time. All sentences before this
    // should be ignored, and any sentences received after this that do
    // not have a date should use the known date.

    QFETCH(QByteArray, bytes);
    QFETCH(QList<QDateTime>, dateTimes);
    QFETCH(QList<bool>, expectHorizontalAccuracy);
    QFETCH(QList<bool>, expectVerticalAccuracy);

    QNmeaPositionInfoSource source(m_mode);
    source.setUserEquivalentRangeError(5.1);
    QNmeaPositionInfoSourceProxyFactory factory;
    QNmeaPositionInfoSourceProxy *proxy = static_cast<QNmeaPositionInfoSourceProxy*>(factory.createProxy(&source));

    QSignalSpy spy(proxy->source(), SIGNAL(positionUpdated(QGeoPositionInfo)));
    proxy->source()->startUpdates();

    proxy->feedBytes(bytes);
    QTRY_COMPARE(spy.count(), dateTimes.count());

    for (int i=0; i<spy.count(); i++) {
        QGeoPositionInfo pInfo = spy[i][0].value<QGeoPositionInfo>();

        QCOMPARE(pInfo.timestamp(), dateTimes[i]);

        // Generated GGA/GSA sentences have hard coded HDOP of 3.5, which corrisponds to a
        // horizontal accuracy of 35.7, for the user equivalent range error of 5.1 set above.
        QCOMPARE(pInfo.hasAttribute(QGeoPositionInfo::HorizontalAccuracy),
                 expectHorizontalAccuracy[i]);
        if (pInfo.hasAttribute(QGeoPositionInfo::HorizontalAccuracy))
            QVERIFY(qFuzzyCompare(pInfo.attribute(QGeoPositionInfo::HorizontalAccuracy), 35.7));

        // Generate GSA sentences have hard coded VDOP of 4.0, which corrisponds to a vertical
        // accuracy of 40.8, for the user equivalent range error of 5.1 set above.
        QCOMPARE(pInfo.hasAttribute(QGeoPositionInfo::VerticalAccuracy),
                 expectVerticalAccuracy[i]);
        if (pInfo.hasAttribute(QGeoPositionInfo::VerticalAccuracy))
            QVERIFY(qFuzzyCompare(pInfo.attribute(QGeoPositionInfo::VerticalAccuracy), 40.8));
    }
}

void tst_QNmeaPositionInfoSource::startUpdates_waitForValidDateTime_data()
{
    QTest::addColumn<QByteArray>("bytes");
    QTest::addColumn<QList<QDateTime> >("dateTimes");
    QTest::addColumn<QList<bool> >("expectHorizontalAccuracy");
    QTest::addColumn<QList<bool> >("expectVerticalAccuracy");

    QDateTime dt = QDateTime::currentDateTime().toUTC();
    QByteArray bytes;

    // should only receive RMC sentence and the GGA sentence *after* it
    bytes += QLocationTestUtils::createGgaSentence(dt.addSecs(1).time()).toLatin1();
    bytes += QLocationTestUtils::createRmcSentence(dt.addSecs(2)).toLatin1();
    bytes += QLocationTestUtils::createGgaSentence(dt.addSecs(3).time()).toLatin1();
    QTest::newRow("Feed GGA,RMC,GGA; expect RMC, second GGA only")
            << bytes << (QList<QDateTime>() << dt.addSecs(2) << dt.addSecs(3))
            << (QList<bool>() << true << true)
            << (QList<bool>() << false << false);

    // should not receive ZDA (has no coordinates) but should get the GGA
    // sentence after it since it got the date/time from ZDA
    bytes.clear();
    bytes += QLocationTestUtils::createGgaSentence(dt.addSecs(1).time()).toLatin1();
    bytes += QLocationTestUtils::createZdaSentence(dt.addSecs(2)).toLatin1();
    bytes += QLocationTestUtils::createGgaSentence(dt.addSecs(3).time()).toLatin1();
    QTest::newRow("Feed GGA,ZDA,GGA; expect second GGA only")
            << bytes << (QList<QDateTime>() << dt.addSecs(3))
            << (QList<bool>() << true)
            << (QList<bool>() << false);

    // Feed ZDA,GGA,GSA,GGA; expect vertical accuracy from second GGA.
    bytes.clear();
    bytes += QLocationTestUtils::createZdaSentence(dt.addSecs(1)).toLatin1();
    bytes += QLocationTestUtils::createGgaSentence(dt.addSecs(2).time()).toLatin1();
    bytes += QLocationTestUtils::createGsaSentence().toLatin1();
    bytes += QLocationTestUtils::createGgaSentence(dt.addSecs(3).time()).toLatin1();
    QTest::newRow("Feed ZDA,GGA,GSA,GGA; expect vertical accuracy from second GGA")
            << bytes << (QList<QDateTime>() << dt.addSecs(2) << dt.addSecs(3))
            << (QList<bool>() << true << true)
            << (QList<bool>() << false << true);

    if (m_mode == QNmeaPositionInfoSource::SimulationMode) {
        // In sim m_mode, should ignore sentence with a date/time before the known date/time
        // (in real time m_mode, everything is passed on regardless)
        bytes.clear();
        bytes += QLocationTestUtils::createRmcSentence(dt.addSecs(1)).toLatin1();
        bytes += QLocationTestUtils::createRmcSentence(dt.addSecs(-2)).toLatin1();
        bytes += QLocationTestUtils::createRmcSentence(dt.addSecs(2)).toLatin1();
        QTest::newRow("Feed good RMC, RMC with bad date/time, good RMC; expect first and third RMC only")
                << bytes << (QList<QDateTime>() << dt.addSecs(1) << dt.addSecs(2))
                << (QList<bool>() << false << false)
                << (QList<bool>() << false << false);
    }
}

void tst_QNmeaPositionInfoSource::requestUpdate_waitForValidDateTime()
{
    QFETCH(QByteArray, bytes);
    QFETCH(QList<QDateTime>, dateTimes);

    QNmeaPositionInfoSource source(m_mode);
    QNmeaPositionInfoSourceProxyFactory factory;
    QNmeaPositionInfoSourceProxy *proxy = static_cast<QNmeaPositionInfoSourceProxy*>(factory.createProxy(&source));

    QSignalSpy spy(proxy->source(), SIGNAL(positionUpdated(QGeoPositionInfo)));
    proxy->source()->requestUpdate();

    proxy->feedBytes(bytes);
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spy[0][0].value<QGeoPositionInfo>().timestamp(), dateTimes[0]);
}

void tst_QNmeaPositionInfoSource::requestUpdate_waitForValidDateTime_data()
{
    startUpdates_waitForValidDateTime_data();
}

void tst_QNmeaPositionInfoSource::requestUpdate()
{
    QNmeaPositionInfoSource source(m_mode);
    QNmeaPositionInfoSourceProxyFactory factory;
    QNmeaPositionInfoSourceProxy *proxy = static_cast<QNmeaPositionInfoSourceProxy*>(factory.createProxy(&source));

    QSignalSpy spyUpdate(proxy->source(), SIGNAL(positionUpdated(QGeoPositionInfo)));
    QSignalSpy spyTimeout(proxy->source(), SIGNAL(updateTimeout()));
    QDateTime dt;

    proxy->source()->requestUpdate(100);
    QTRY_COMPARE(spyTimeout.count(), 1);
    spyTimeout.clear();

    dt = QDateTime::currentDateTime().toUTC();
    proxy->feedUpdate(dt);
    proxy->source()->requestUpdate();
    QTRY_COMPARE(spyUpdate.count(), 1);
    QCOMPARE(spyUpdate[0][0].value<QGeoPositionInfo>().timestamp(), dt);
    QCOMPARE(spyTimeout.count(), 0);
    spyUpdate.clear();

    // delay the update and expect it to be emitted after 300ms
    dt = QDateTime::currentDateTime().toUTC();
    proxy->source()->requestUpdate(1000);
    QTest::qWait(300);
    proxy->feedUpdate(dt);
    QTRY_COMPARE(spyUpdate.count(), 1);
    QCOMPARE(spyUpdate[0][0].value<QGeoPositionInfo>().timestamp(), dt);
    QCOMPARE(spyTimeout.count(), 0);
    spyUpdate.clear();

    // delay the update and expect updateTimeout() to be emitted
    dt = QDateTime::currentDateTime().toUTC();
    proxy->source()->requestUpdate(500);
    QTest::qWait(1000);
    proxy->feedUpdate(dt);
    QCOMPARE(spyTimeout.count(), 1);
    QCOMPARE(spyUpdate.count(), 0);
    spyUpdate.clear();
}

void tst_QNmeaPositionInfoSource::requestUpdate_after_start()
{
    QNmeaPositionInfoSource source(m_mode);
    QNmeaPositionInfoSourceProxyFactory factory;
    QNmeaPositionInfoSourceProxy *proxy = static_cast<QNmeaPositionInfoSourceProxy*>(factory.createProxy(&source));

    QSignalSpy spyUpdate(proxy->source(), SIGNAL(positionUpdated(QGeoPositionInfo)));
    QSignalSpy spyTimeout(proxy->source(), SIGNAL(updateTimeout()));

    // Start updates with 500ms interval and requestUpdate() with 100ms
    // timeout. Feed an update, and it should be emitted immediately due to
    // the requestUpdate(). The update should not be emitted again after that
    // (i.e. the startUpdates() interval should not cause it to be re-emitted).

    QDateTime dt = QDateTime::currentDateTime().toUTC();
    proxy->source()->setUpdateInterval(500);
    proxy->source()->startUpdates();
    proxy->source()->requestUpdate(100);
    proxy->feedUpdate(dt);
    QTRY_COMPARE(spyUpdate.count(), 1);
    QCOMPARE(spyUpdate[0][0].value<QGeoPositionInfo>().timestamp(), dt);
    QCOMPARE(spyTimeout.count(), 0);
    spyUpdate.clear();

    // Update has been emitted for requestUpdate(), shouldn't be emitted for startUpdates()
    QTRY_COMPARE_WITH_TIMEOUT(spyUpdate.count(), 0, 1000);
}

void tst_QNmeaPositionInfoSource::testWithBadNmea()
{
    QFETCH(QByteArray, bytes);
    QFETCH(QList<QDateTime>, dateTimes);
    QFETCH(UpdateTriggerMethod, trigger);

    QNmeaPositionInfoSource source(m_mode);
    QNmeaPositionInfoSourceProxyFactory factory;
    QNmeaPositionInfoSourceProxy *proxy = static_cast<QNmeaPositionInfoSourceProxy*>(factory.createProxy(&source));

    QSignalSpy spy(proxy->source(), SIGNAL(positionUpdated(QGeoPositionInfo)));
    if (trigger == StartUpdatesMethod)
        proxy->source()->startUpdates();
    else
        proxy->source()->requestUpdate();

    proxy->feedBytes(bytes);
    QTRY_COMPARE(spy.count(), dateTimes.count());
    for (int i=0; i<dateTimes.count(); i++)
        QCOMPARE(spy[i][0].value<QGeoPositionInfo>().timestamp(), dateTimes[i]);
}

void tst_QNmeaPositionInfoSource::testWithBadNmea_data()
{
    QTest::addColumn<QByteArray>("bytes");
    QTest::addColumn<QList<QDateTime> >("dateTimes");
    QTest::addColumn<UpdateTriggerMethod>("trigger");

    QDateTime firstDateTime = QDateTime::currentDateTime().toUTC();
    QByteArray bad = QLocationTestUtils::createRmcSentence(firstDateTime.addSecs(1)).toLatin1();
    bad = bad.mid(bad.length()/2);
    QDateTime lastDateTime = firstDateTime.addSecs(2);

    QByteArray bytes;
    bytes += QLocationTestUtils::createRmcSentence(firstDateTime).toLatin1();
    bytes += bad;
    bytes += QLocationTestUtils::createRmcSentence(lastDateTime).toLatin1();
    QTest::newRow("requestUpdate(), bad second sentence") << bytes
            << (QList<QDateTime>() << firstDateTime) << RequestUpdatesMethod;
    QTest::newRow("startUpdates(), bad second sentence") << bytes
            << (QList<QDateTime>() << firstDateTime << lastDateTime) << StartUpdatesMethod;
}
