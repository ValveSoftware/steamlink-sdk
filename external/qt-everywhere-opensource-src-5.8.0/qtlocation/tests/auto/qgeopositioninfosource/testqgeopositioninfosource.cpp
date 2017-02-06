/****************************************************************************
**
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

#include <QTest>
#include <QMetaType>
#include <QSignalSpy>
#include <QDebug>
#include <QTimer>

#include <limits.h>

#include <qnumeric.h>
#include <QtPositioning/qgeopositioninfosource.h>
#include <QtPositioning/qgeopositioninfo.h>

#include "testqgeopositioninfosource_p.h"
#include "../utils/qlocationtestutils_p.h"

Q_DECLARE_METATYPE(QGeoPositionInfoSource::PositioningMethod)
Q_DECLARE_METATYPE(QGeoPositionInfoSource::PositioningMethods)
Q_DECLARE_METATYPE(QGeoPositionInfo)

#define MAX_WAITING_TIME 50000

// Must provide a valid source, unless testing the source
// returned by QGeoPositionInfoSource::createDefaultSource() on a system
// that has no default source
#define CHECK_SOURCE_VALID { \
        if (!m_source) { \
            if (m_testingDefaultSource && QGeoPositionInfoSource::createDefaultSource(0) == 0) \
                QSKIP("No default position source on this system"); \
            else \
                QFAIL("createTestSource() must return a valid source!"); \
        } \
    }

class MyPositionSource : public QGeoPositionInfoSource
{
    Q_OBJECT
public:
    MyPositionSource(QObject *parent = 0)
            : QGeoPositionInfoSource(parent) {
    }

    QGeoPositionInfo lastKnownPosition(bool /*fromSatellitePositioningMethodsOnly = false*/) const {
        return QGeoPositionInfo();
    }

    void setSupportedPositioningMethods(PositioningMethods methods) {
        m_methods = methods;
    }

    virtual PositioningMethods supportedPositioningMethods() const {
        return m_methods;
    }
    virtual int minimumUpdateInterval() const {
        return 0;
    }

    virtual void startUpdates() {}
    virtual void stopUpdates() {}

    virtual void requestUpdate(int) {}

    Error error() const { return QGeoPositionInfoSource::NoError; }

private:
    PositioningMethods m_methods;
};

class DefaultSourceTest : public TestQGeoPositionInfoSource
{
    Q_OBJECT
protected:
    QGeoPositionInfoSource *createTestSource() {
        return QGeoPositionInfoSource::createSource(QStringLiteral("test.source"), 0);
    }
};


TestQGeoPositionInfoSource::TestQGeoPositionInfoSource(QObject *parent)
        : QObject(parent)
{
    m_testingDefaultSource = false;
    /*
     * Set custom path since CI doesn't install test plugins
     */
#ifdef Q_OS_WIN
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath() +
                                     QStringLiteral("/../../../../plugins"));
#else
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath()
                                     + QStringLiteral("/../../../plugins"));
#endif
}

TestQGeoPositionInfoSource *TestQGeoPositionInfoSource::createDefaultSourceTest()
{
    DefaultSourceTest *test = new DefaultSourceTest;
    test->m_testingDefaultSource = true;
    return test;
}

void TestQGeoPositionInfoSource::test_slot1()
{
}

void TestQGeoPositionInfoSource::test_slot2()
{
    m_testSlot2Called = true;
}

void TestQGeoPositionInfoSource::base_initTestCase()
{
    qRegisterMetaType<QGeoPositionInfo>();
}

void TestQGeoPositionInfoSource::base_init()
{
    m_source = createTestSource();
    m_testSlot2Called = false;
}

void TestQGeoPositionInfoSource::base_cleanup()
{
    delete m_source;
    m_source = 0;
}

void TestQGeoPositionInfoSource::base_cleanupTestCase()
{
}

void TestQGeoPositionInfoSource::initTestCase()
{
    base_initTestCase();
}

void TestQGeoPositionInfoSource::init()
{
    base_init();
}

void TestQGeoPositionInfoSource::cleanup()
{
    base_cleanup();
}

void TestQGeoPositionInfoSource::cleanupTestCase()
{
    base_cleanupTestCase();
}

// TC_ID_3_x_1
void TestQGeoPositionInfoSource::constructor_withParent()
{
    QObject *parent = new QObject();
    new MyPositionSource(parent);
    delete parent;
}

// TC_ID_3_x_2
void TestQGeoPositionInfoSource::constructor_noParent()
{
    MyPositionSource *obj = new MyPositionSource();
    delete obj;
}

void TestQGeoPositionInfoSource::updateInterval()
{
    MyPositionSource s;
    QCOMPARE(s.updateInterval(), 0);
}

void TestQGeoPositionInfoSource::setPreferredPositioningMethods()
{
    QFETCH(QGeoPositionInfoSource::PositioningMethod, supported);
    QFETCH(QGeoPositionInfoSource::PositioningMethod, preferred);
    QFETCH(QGeoPositionInfoSource::PositioningMethod, resulting);

    MyPositionSource s;
    s.setSupportedPositioningMethods(supported);
    s.setPreferredPositioningMethods(preferred);
    QCOMPARE(s.preferredPositioningMethods(), resulting);
}

void TestQGeoPositionInfoSource::setPreferredPositioningMethods_data()
{
    QTest::addColumn<QGeoPositionInfoSource::PositioningMethod>("supported");
    QTest::addColumn<QGeoPositionInfoSource::PositioningMethod>("preferred");
    QTest::addColumn<QGeoPositionInfoSource::PositioningMethod>("resulting");

    QTest::newRow("Sat supported, Sat preferred")
        << QGeoPositionInfoSource::SatellitePositioningMethods
        << QGeoPositionInfoSource::SatellitePositioningMethods
        << QGeoPositionInfoSource::SatellitePositioningMethods;
    QTest::newRow("Sat supported, Non-Sat preferred")
        << QGeoPositionInfoSource::SatellitePositioningMethods
        << QGeoPositionInfoSource::NonSatellitePositioningMethods
        << QGeoPositionInfoSource::SatellitePositioningMethods;
    QTest::newRow("Sat supported, All preferred")
        << QGeoPositionInfoSource::SatellitePositioningMethods
        << QGeoPositionInfoSource::AllPositioningMethods
        << QGeoPositionInfoSource::SatellitePositioningMethods;

    QTest::newRow("Non-Sat supported, Sat preferred")
        << QGeoPositionInfoSource::NonSatellitePositioningMethods
        << QGeoPositionInfoSource::SatellitePositioningMethods
        << QGeoPositionInfoSource::NonSatellitePositioningMethods;
    QTest::newRow("Non-Sat supported, Non-Sat preferred")
        << QGeoPositionInfoSource::NonSatellitePositioningMethods
        << QGeoPositionInfoSource::NonSatellitePositioningMethods
        << QGeoPositionInfoSource::NonSatellitePositioningMethods;
    QTest::newRow("Non-Sat supported, All preferred")
        << QGeoPositionInfoSource::NonSatellitePositioningMethods
        << QGeoPositionInfoSource::AllPositioningMethods
        << QGeoPositionInfoSource::NonSatellitePositioningMethods;

    QTest::newRow("All supported, Sat preferred")
        << QGeoPositionInfoSource::AllPositioningMethods
        << QGeoPositionInfoSource::SatellitePositioningMethods
        << QGeoPositionInfoSource::SatellitePositioningMethods;
    QTest::newRow("All supported, Non-Sat preferred")
        << QGeoPositionInfoSource::AllPositioningMethods
        << QGeoPositionInfoSource::NonSatellitePositioningMethods
        << QGeoPositionInfoSource::NonSatellitePositioningMethods;
    QTest::newRow("All supported, All preferred")
        << QGeoPositionInfoSource::AllPositioningMethods
        << QGeoPositionInfoSource::AllPositioningMethods
        << QGeoPositionInfoSource::AllPositioningMethods;
}

void TestQGeoPositionInfoSource::preferredPositioningMethods()
{
    MyPositionSource s;
    QCOMPARE(s.preferredPositioningMethods(), 0);
}

//TC_ID_3_x_1 : Create a position source with the given parent that reads from the system's default
// sources of location data
void TestQGeoPositionInfoSource::createDefaultSource()
{
    QObject *parent = new QObject;

    QGeoPositionInfoSource *source = QGeoPositionInfoSource::createDefaultSource(parent);
    // now all platforms have the dummy plugin at least
    QVERIFY(source != 0);
    delete parent;
}

void TestQGeoPositionInfoSource::setUpdateInterval()
{
    CHECK_SOURCE_VALID;

    QFETCH(int, interval);
    QFETCH(int, expectedInterval);

    m_source->setUpdateInterval(interval);
    QCOMPARE(m_source->updateInterval(), expectedInterval);
}

void TestQGeoPositionInfoSource::setUpdateInterval_data()
{
    QTest::addColumn<int>("interval");
    QTest::addColumn<int>("expectedInterval");
    QGeoPositionInfoSource *source = createTestSource();
    int minUpdateInterval = source ? source->minimumUpdateInterval() : -1;
    if (source)
        delete source;

    QTest::newRow("0") << 0 << 0;

    if (minUpdateInterval > -1) {
        QTest::newRow("INT_MIN") << INT_MIN << minUpdateInterval;
        QTest::newRow("-1") << -1 << minUpdateInterval;
    }

    if (minUpdateInterval > 0) {
        QTest::newRow("more than minInterval") << minUpdateInterval + 1 << minUpdateInterval + 1;
        QTest::newRow("equal to minInterval") << minUpdateInterval << minUpdateInterval;
    }

    if (minUpdateInterval > 1) {
        QTest::newRow("less then minInterval") << minUpdateInterval - 1 << minUpdateInterval;
        QTest::newRow("in btw zero and minInterval") << 1 << minUpdateInterval;
    }
}

void TestQGeoPositionInfoSource::lastKnownPosition()
{
    CHECK_SOURCE_VALID;
    QFETCH(QGeoPositionInfoSource::PositioningMethod, positioningMethod);
    QFETCH(bool, lastKnownPositionArgument);

    if ((m_source->supportedPositioningMethods() & positioningMethod) == 0)
        QSKIP("Not a supported positioning method for this position source");

    m_source->setPreferredPositioningMethods(positioningMethod);

    QSignalSpy spy(m_source, SIGNAL(positionUpdated(QGeoPositionInfo)));
    QSignalSpy timeout(m_source, SIGNAL(updateTimeout()));
    int time_out = 7000;
    m_source->setUpdateInterval(time_out);
    m_source->startUpdates();

    // Use QEventLoop instead of qWait() to ensure we stop as soon as a
    // position is emitted (otherwise the lastKnownPosition() may have
    // changed by the time it is checked)
    QEventLoop loop;
    QTimer timer;
    //simulated CI tests will quickly return -> real GPS tests take 2 minutes for satellite systems
    //use a 5 min timeout
    timer.setInterval(300000);
    connect(m_source, SIGNAL(positionUpdated(QGeoPositionInfo)),
            &loop, SLOT(quit()));
    connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    timer.start();
    loop.exec();

    QVERIFY((spy.count() > 0) && (timeout.count() == 0));

    QList<QVariant> list = spy.takeFirst();
    QGeoPositionInfo info = list.at(0).value<QGeoPositionInfo>();
    QGeoPositionInfo lastPositioninfo = m_source->lastKnownPosition(lastKnownPositionArgument);

    // lastPositioninfo is only gauranteed to be valid in all cases when only using satelite
    // positioning methods or when lastKnownPositionArgument is false
    if (!lastKnownPositionArgument ||
        positioningMethod == QGeoPositionInfoSource::SatellitePositioningMethods) {
        QVERIFY(lastPositioninfo.isValid());
    }

    if (lastPositioninfo.isValid()) {
        QCOMPARE(info.coordinate(), lastPositioninfo.coordinate());
        // On some CI machines the above evenloop code is not sufficient as positionUpdated
        // still fires causing last know position and last update to be out of sync.
        // To accommodate we check that the time stamps are no more than 1s apart
        // ideally they should be the same
        // doesn't work: QCOMPARE(info.timestamp(), lastPositioninfo.timestamp());
        const qint64 diff = qAbs(info.timestamp().msecsTo(lastPositioninfo.timestamp()));
        QCOMPARE(diff < 1000, true);

        QCOMPARE(info.hasAttribute(QGeoPositionInfo::HorizontalAccuracy),
                 lastPositioninfo.hasAttribute(QGeoPositionInfo::HorizontalAccuracy));

        if (info.hasAttribute(QGeoPositionInfo::HorizontalAccuracy)) {
            bool isNaN1 =  qIsNaN(info.attribute(QGeoPositionInfo::HorizontalAccuracy));
            bool isNaN2 =  qIsNaN(lastPositioninfo.attribute(QGeoPositionInfo::HorizontalAccuracy));
            QCOMPARE(isNaN1, isNaN2);
            if (!isNaN1) {
                QCOMPARE(qFuzzyCompare(info.attribute(QGeoPositionInfo::HorizontalAccuracy),
                                       lastPositioninfo.attribute(QGeoPositionInfo::HorizontalAccuracy)), true);
            }
        }

        QCOMPARE(info.hasAttribute(QGeoPositionInfo::VerticalAccuracy),
                 lastPositioninfo.hasAttribute(QGeoPositionInfo::VerticalAccuracy));

        if (info.hasAttribute(QGeoPositionInfo::VerticalAccuracy)) {
            bool isNaN1 =  qIsNaN(info.attribute(QGeoPositionInfo::VerticalAccuracy));
            bool isNaN2 =  qIsNaN(lastPositioninfo.attribute(QGeoPositionInfo::VerticalAccuracy));
            QCOMPARE(isNaN1, isNaN2);
            if (!isNaN1) {
                QCOMPARE(qFuzzyCompare(info.attribute(QGeoPositionInfo::VerticalAccuracy),
                                       lastPositioninfo.attribute(QGeoPositionInfo::VerticalAccuracy)), true);
            }
        }
    }

    m_source->stopUpdates();
}

void TestQGeoPositionInfoSource::lastKnownPosition_data()
{
    QTest::addColumn<QGeoPositionInfoSource::PositioningMethod>("positioningMethod");
    QTest::addColumn<bool>("lastKnownPositionArgument");

    // no good way to determine on MeeGo what are supported. If we ask for all or non-satellites, we
    // typically get geoclue-example provider, which is not suitable for this test.
    QTest::newRow("all - false") << QGeoPositionInfoSource::AllPositioningMethods << false;
    QTest::newRow("all - true") << QGeoPositionInfoSource::AllPositioningMethods << true;
    QTest::newRow("satellite - false") << QGeoPositionInfoSource::SatellitePositioningMethods << false;
    QTest::newRow("satellite - true") << QGeoPositionInfoSource::SatellitePositioningMethods << true;
}

void TestQGeoPositionInfoSource::minimumUpdateInterval()
{
    CHECK_SOURCE_VALID;

    QVERIFY(m_source->minimumUpdateInterval() > 0);
}

//TC_ID_3_x_1
void TestQGeoPositionInfoSource::startUpdates_testIntervals()
{
    CHECK_SOURCE_VALID;
    QSignalSpy spy(m_source, SIGNAL(positionUpdated(QGeoPositionInfo)));
    QSignalSpy timeout(m_source, SIGNAL(updateTimeout()));
    m_source->setUpdateInterval(7000);
    int interval = m_source->updateInterval();

    m_source->startUpdates();

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 9500);
    for (int i = 0; i < 6; i++) {
        QTRY_VERIFY_WITH_TIMEOUT((spy.count() == 1) && (timeout.count() == 0), (interval*2));
        spy.clear();
    }

    m_source->stopUpdates();
}


void TestQGeoPositionInfoSource::startUpdates_testIntervalChangesWhileRunning()
{
    // There are two ways of dealing with an interval change, and we have left it system dependent.
    // The interval can be changed will running or after the next update.
    // WinCE uses the first method, S60 uses the second method.

    // The minimum interval on the symbian emulator is 5000 msecs, which is why the times in
    // this test are as high as they are.

    CHECK_SOURCE_VALID;

    QSignalSpy spy(m_source, SIGNAL(positionUpdated(QGeoPositionInfo)));
    QSignalSpy timeout(m_source, SIGNAL(updateTimeout()));
    m_source->setUpdateInterval(0);
    m_source->startUpdates();
    m_source->setUpdateInterval(0);

    QTRY_VERIFY_WITH_TIMEOUT(spy.count() > 0, 7000);
    QCOMPARE(timeout.count(), 0);
    spy.clear();

    m_source->setUpdateInterval(5000);

    QTRY_VERIFY_WITH_TIMEOUT((spy.count() == 2) && (timeout.count() == 0), 15000);
    spy.clear();

    m_source->setUpdateInterval(10000);

    QTRY_VERIFY_WITH_TIMEOUT((spy.count() == 2) && (timeout.count() == 0), 30000);
    spy.clear();

    m_source->setUpdateInterval(5000);

    QTRY_VERIFY_WITH_TIMEOUT((spy.count() == 2) && (timeout.count() == 0), 15000);
    spy.clear();

    m_source->setUpdateInterval(5000);

    QTRY_VERIFY_WITH_TIMEOUT((spy.count() == 2) && (timeout.count() == 0), 15000);
    spy.clear();

    m_source->setUpdateInterval(0);

    QTRY_VERIFY_WITH_TIMEOUT((spy.count() == 1) && (timeout.count() == 0), 7000);
    spy.clear();

    m_source->setUpdateInterval(0);

    QTRY_VERIFY_WITH_TIMEOUT((spy.count() == 1) && (timeout.count() == 0), 7000);
    spy.clear();

    m_source->stopUpdates();
}

//TC_ID_3_x_2
void TestQGeoPositionInfoSource::startUpdates_testDefaultInterval()
{
    CHECK_SOURCE_VALID;

    QSignalSpy spy(m_source, SIGNAL(positionUpdated(QGeoPositionInfo)));
    QSignalSpy timeout(m_source, SIGNAL(updateTimeout()));
    m_source->startUpdates();
    for (int i = 0; i < 3; i++) {

        QTRY_VERIFY_WITH_TIMEOUT((spy.count() > 0) && (timeout.count() == 0), 7000);
        spy.clear();
    }
    m_source->stopUpdates();
}

//TC_ID_3_x_3
void TestQGeoPositionInfoSource::startUpdates_testZeroInterval()
{
    CHECK_SOURCE_VALID;

    QSignalSpy spy(m_source, SIGNAL(positionUpdated(QGeoPositionInfo)));
    QSignalSpy timeout(m_source, SIGNAL(updateTimeout()));
    m_source->setUpdateInterval(0);
    m_source->startUpdates();
    for (int i = 0; i < 3; i++) {
        QTRY_VERIFY_WITH_TIMEOUT((spy.count() > 0) && (timeout.count() == 0), 7000);
        spy.clear();
    }
    m_source->stopUpdates();
}

void TestQGeoPositionInfoSource::startUpdates_moreThanOnce()
{
    CHECK_SOURCE_VALID;

    QSignalSpy spy(m_source, SIGNAL(positionUpdated(QGeoPositionInfo)));
    QSignalSpy timeout(m_source, SIGNAL(updateTimeout()));
    m_source->setUpdateInterval(0);
    m_source->startUpdates();

    m_source->startUpdates(); // check there is no crash

    QTRY_VERIFY_WITH_TIMEOUT((spy.count() > 0) && (timeout.count() == 0), 7000);

    m_source->startUpdates(); // check there is no crash

    m_source->stopUpdates();
}

//TC_ID_3_x_1
void TestQGeoPositionInfoSource::stopUpdates()
{
    CHECK_SOURCE_VALID;

    QSignalSpy spy(m_source, SIGNAL(positionUpdated(QGeoPositionInfo)));
    QSignalSpy timeout(m_source, SIGNAL(updateTimeout()));
    m_source->setUpdateInterval(7000);
    m_source->startUpdates();
    for (int i = 0; i < 2; i++) {
        QTRY_VERIFY_WITH_TIMEOUT((spy.count() > 0) && (timeout.count() == 0), 9500);
        spy.clear();
    }
    m_source->stopUpdates();
    QTest::qWait(9500);
    QCOMPARE(spy.count(), 0);
    spy.clear();

    m_source->setUpdateInterval(0);
    m_source->startUpdates();
    m_source->stopUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 0, 9500);
}

//TC_ID_3_x_2
void TestQGeoPositionInfoSource::stopUpdates_withoutStart()
{
    CHECK_SOURCE_VALID;
    m_source->stopUpdates(); // check there is no crash
}

void TestQGeoPositionInfoSource::requestUpdate()
{
    CHECK_SOURCE_VALID;
    QFETCH(int, timeout);
    QSignalSpy spy(m_source, SIGNAL(updateTimeout()));
    m_source->requestUpdate(timeout);
    QTRY_COMPARE(spy.count(), 1);
}

void TestQGeoPositionInfoSource::requestUpdate_data()
{
    QTest::addColumn<int>("timeout");
    QTest::newRow("less than zero") << -1;          //TC_ID_3_x_7
}

// TC_ID_3_x_1 : Create position source and call requestUpdate with valid timeout value
void TestQGeoPositionInfoSource::requestUpdate_validTimeout()
{
    CHECK_SOURCE_VALID;

    QSignalSpy spyUpdate(m_source, SIGNAL(positionUpdated(QGeoPositionInfo)));
    QSignalSpy spyTimeout(m_source, SIGNAL(updateTimeout()));

    m_source->requestUpdate(7000);

    QTRY_VERIFY_WITH_TIMEOUT((spyUpdate.count() > 0) && (spyTimeout.count() == 0), 7000);
}

void TestQGeoPositionInfoSource::requestUpdate_defaultTimeout()
{
    CHECK_SOURCE_VALID;
    QSignalSpy spyUpdate(m_source, SIGNAL(positionUpdated(QGeoPositionInfo)));
    QSignalSpy spyTimeout(m_source, SIGNAL(updateTimeout()));

    m_source->requestUpdate(0);

    QTRY_VERIFY_WITH_TIMEOUT((spyUpdate.count() > 0) && (spyTimeout.count() == 0), 7000);
}

// TC_ID_3_x_2 : Create position source and call requestUpdate with a timeout less than
// minimumupdateInterval
void TestQGeoPositionInfoSource::requestUpdate_timeoutLessThanMinimumInterval()
{
    CHECK_SOURCE_VALID;

    QSignalSpy spyTimeout(m_source, SIGNAL(updateTimeout()));
    m_source->requestUpdate(1);

    QTRY_COMPARE_WITH_TIMEOUT(spyTimeout.count(), 1, 1000);
}

// TC_ID_3_x_3 : Call requestUpdate() with same value repeatedly
void TestQGeoPositionInfoSource::requestUpdate_repeatedCalls()
{
    CHECK_SOURCE_VALID;

    QSignalSpy spyUpdate(m_source, SIGNAL(positionUpdated(QGeoPositionInfo)));
    QSignalSpy spyTimeout(m_source, SIGNAL(updateTimeout()));

    m_source->requestUpdate(7000);

    QTRY_VERIFY_WITH_TIMEOUT((spyUpdate.count() > 0) && (spyTimeout.count() == 0), 7000);
    spyUpdate.clear();
    m_source->requestUpdate(7000);

    QTRY_VERIFY_WITH_TIMEOUT((spyUpdate.count() > 0) && (spyTimeout.count() == 0), 7000);
}

void TestQGeoPositionInfoSource::requestUpdate_overlappingCalls()
{
    CHECK_SOURCE_VALID;

    QSignalSpy spyUpdate(m_source, SIGNAL(positionUpdated(QGeoPositionInfo)));
    QSignalSpy spyTimeout(m_source, SIGNAL(updateTimeout()));

    m_source->requestUpdate(7000);
    m_source->requestUpdate(7000);

    QTRY_VERIFY_WITH_TIMEOUT((spyUpdate.count() > 0) && (spyTimeout.count() == 0), 7000);
}

//TC_ID_3_x_4
void TestQGeoPositionInfoSource::requestUpdateAfterStartUpdates_ZeroInterval()
{
    CHECK_SOURCE_VALID;

    QSignalSpy spyUpdate(m_source, SIGNAL(positionUpdated(QGeoPositionInfo)));
    QSignalSpy spyTimeout(m_source, SIGNAL(updateTimeout()));

    m_source->setUpdateInterval(0);
    m_source->startUpdates();

    QTRY_VERIFY_WITH_TIMEOUT((spyUpdate.count() > 0) && (spyTimeout.count() == 0), 7000);
    spyUpdate.clear();

    m_source->requestUpdate(7000);
    QTest::qWait(7000);

    QVERIFY((spyUpdate.count() > 0) && (spyTimeout.count() == 0));
    spyUpdate.clear();

    QTRY_VERIFY_WITH_TIMEOUT((spyUpdate.count() > 0) && (spyTimeout.count() == 0), MAX_WAITING_TIME);

    m_source->stopUpdates();
}

void TestQGeoPositionInfoSource::requestUpdateAfterStartUpdates_SmallInterval()
{
    CHECK_SOURCE_VALID;

    QSignalSpy spyUpdate(m_source, SIGNAL(positionUpdated(QGeoPositionInfo)));
    QSignalSpy spyTimeout(m_source, SIGNAL(updateTimeout()));

    m_source->setUpdateInterval(10000);
    m_source->startUpdates();

    QTRY_VERIFY_WITH_TIMEOUT((spyUpdate.count() == 1) && (spyTimeout.count() == 0), 20000);
    spyUpdate.clear();

    m_source->requestUpdate(7000);

    QTRY_VERIFY_WITH_TIMEOUT((spyUpdate.count() == 1) && (spyTimeout.count() == 0), 7000);
    spyUpdate.clear();

    QTRY_VERIFY_WITH_TIMEOUT((spyUpdate.count() == 1) && (spyTimeout.count() == 0), 20000);

    m_source->stopUpdates();
}

void TestQGeoPositionInfoSource::requestUpdateBeforeStartUpdates_ZeroInterval()
{
    CHECK_SOURCE_VALID;

    QSignalSpy spyUpdate(m_source, SIGNAL(positionUpdated(QGeoPositionInfo)));
    QSignalSpy spyTimeout(m_source, SIGNAL(updateTimeout()));

    m_source->requestUpdate(7000);

    m_source->setUpdateInterval(0);
    m_source->startUpdates();

    QTRY_VERIFY_WITH_TIMEOUT((spyUpdate.count() >= 2) && (spyTimeout.count() == 0), 14000);
    spyUpdate.clear();

    QTest::qWait(7000);

    QCOMPARE(spyTimeout.count(), 0);

    m_source->stopUpdates();
}

void TestQGeoPositionInfoSource::requestUpdateBeforeStartUpdates_SmallInterval()
{
    CHECK_SOURCE_VALID;
    QSignalSpy spyUpdate(m_source, SIGNAL(positionUpdated(QGeoPositionInfo)));
    QSignalSpy spyTimeout(m_source, SIGNAL(updateTimeout()));

    m_source->requestUpdate(7000);

    m_source->setUpdateInterval(10000);
    m_source->startUpdates();

    QTRY_VERIFY_WITH_TIMEOUT((spyUpdate.count() > 0) && (spyTimeout.count() == 0), 7000);
    spyUpdate.clear();

    QTRY_VERIFY_WITH_TIMEOUT((spyUpdate.count() > 0) && (spyTimeout.count() == 0), 20000);

    m_source->stopUpdates();
}

void TestQGeoPositionInfoSource::removeSlotForRequestTimeout()
{
    CHECK_SOURCE_VALID;

    bool i = connect(m_source, SIGNAL(updateTimeout()), this, SLOT(test_slot1()));
    QVERIFY(i == true);
    i = connect(m_source, SIGNAL(updateTimeout()), this, SLOT(test_slot2()));
    QVERIFY(i == true);
    i = disconnect(m_source, SIGNAL(updateTimeout()), this, SLOT(test_slot1()));
    QVERIFY(i == true);

    m_source->requestUpdate(-1);
    QTRY_VERIFY_WITH_TIMEOUT((m_testSlot2Called == true), 1000);
}

void TestQGeoPositionInfoSource::removeSlotForPositionUpdated()
{
    CHECK_SOURCE_VALID;

    bool i = connect(m_source, SIGNAL(positionUpdated(QGeoPositionInfo)), this, SLOT(test_slot1()));
    QVERIFY(i == true);
    i = connect(m_source, SIGNAL(positionUpdated(QGeoPositionInfo)), this, SLOT(test_slot2()));
    QVERIFY(i == true);
    i = disconnect(m_source, SIGNAL(positionUpdated(QGeoPositionInfo)), this, SLOT(test_slot1()));
    QVERIFY(i == true);

    m_source->requestUpdate(7000);

    QTRY_VERIFY_WITH_TIMEOUT((m_testSlot2Called == true), 7000);
}

#include "testqgeopositioninfosource.moc"
