/*********************f*******************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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

#include <qgeosatelliteinfosource.h>
#include <qgeosatelliteinfo.h>

#include "testqgeosatelliteinfosource_p.h"
#include "../utils/qlocationtestutils_p.h"

Q_DECLARE_METATYPE(QList<QGeoSatelliteInfo>)

#define MAX_WAITING_TIME 50000

// Must provide a valid source, unless testing the source
// returned by QGeoSatelliteInfoSource::createDefaultSource() on a system
// that has no default source
#define CHECK_SOURCE_VALID { \
        if (!m_source) { \
            if (m_testingDefaultSource && QGeoSatelliteInfoSource::createDefaultSource(0) == 0) \
                QSKIP("No default satellite source on this system"); \
            else \
                QFAIL("createTestSource() must return a valid source!"); \
        } \
    }

class MySatelliteSource : public QGeoSatelliteInfoSource
{
    Q_OBJECT
public:
    MySatelliteSource(QObject *parent = 0)
            : QGeoSatelliteInfoSource(parent) {
    }
    virtual void startUpdates() {}
    virtual void stopUpdates() {}
    virtual void requestUpdate(int) {}
    virtual int minimumUpdateInterval() const {
        return 0;
    }
    Error error() const { return QGeoSatelliteInfoSource::NoError; }
};


class DefaultSourceTest : public TestQGeoSatelliteInfoSource
{
    Q_OBJECT
protected:
    QGeoSatelliteInfoSource *createTestSource() {
        return QGeoSatelliteInfoSource::createDefaultSource(0);
    }
};

TestQGeoSatelliteInfoSource::TestQGeoSatelliteInfoSource(QObject *parent)
        : QObject(parent)
{
    m_testingDefaultSource = false;
}

TestQGeoSatelliteInfoSource *TestQGeoSatelliteInfoSource::createDefaultSourceTest()
{
    DefaultSourceTest *test = new DefaultSourceTest;
    test->m_testingDefaultSource = true;
    return test;
}

void TestQGeoSatelliteInfoSource::base_initTestCase()
{
    qRegisterMetaType<QList<QGeoSatelliteInfo> >();
}

void TestQGeoSatelliteInfoSource::base_init()
{
    m_source = createTestSource();
    m_testSlot2Called = false;
}

void TestQGeoSatelliteInfoSource::base_cleanup()
{
    delete m_source;
    m_source = 0;
}

void TestQGeoSatelliteInfoSource::base_cleanupTestCase()
{
}

void TestQGeoSatelliteInfoSource::initTestCase()
{
    base_initTestCase();
}

void TestQGeoSatelliteInfoSource::init()
{
    base_init();
}

void TestQGeoSatelliteInfoSource::cleanup()
{
    base_cleanup();
}

void TestQGeoSatelliteInfoSource::cleanupTestCase()
{
    base_cleanupTestCase();
}

void TestQGeoSatelliteInfoSource::test_slot1()
{
}

void TestQGeoSatelliteInfoSource::test_slot2()
{
    m_testSlot2Called = true;
}

void TestQGeoSatelliteInfoSource::constructor_withParent()
{
    QObject *parent = new QObject();
    new MySatelliteSource(parent);
    delete parent;
}

void TestQGeoSatelliteInfoSource::constructor_noParent()
{
    MySatelliteSource *obj = new MySatelliteSource();
    delete obj;
}

void TestQGeoSatelliteInfoSource::createDefaultSource()
{
    QObject *parent = new QObject;
    QGeoSatelliteInfoSource *source = QGeoSatelliteInfoSource::createDefaultSource(parent);
    // Satellite sources are available when SATELLITE_SOURCE_AVAILABLE is defined
#if defined(SATELLITE_SOURCE_AVAILABLE)
    QVERIFY(source != 0);
#else
    QVERIFY(source == 0);
#endif
    delete parent;
}

void TestQGeoSatelliteInfoSource::createDefaultSource_noParent()
{
    QGeoSatelliteInfoSource *source = QGeoSatelliteInfoSource::createDefaultSource(0);
#if defined(SATELLITE_SOURCE_AVAILABLE)
    QVERIFY(source != 0);
#else
    QVERIFY(source == 0);
#endif
    delete source;
}

void TestQGeoSatelliteInfoSource::updateInterval()
{
    MySatelliteSource s;
    QCOMPARE(s.updateInterval(), 0);
}

void TestQGeoSatelliteInfoSource::setUpdateInterval()
{
    CHECK_SOURCE_VALID;

    QFETCH(int, interval);
    QFETCH(int, expectedInterval);

    m_source->setUpdateInterval(interval);
    QCOMPARE(m_source->updateInterval(), expectedInterval);
}

void TestQGeoSatelliteInfoSource::setUpdateInterval_data()
{
    QTest::addColumn<int>("interval");
    QTest::addColumn<int>("expectedInterval");
    QGeoSatelliteInfoSource *source = createTestSource();
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

void TestQGeoSatelliteInfoSource::minimumUpdateInterval()
{
    CHECK_SOURCE_VALID;

    QVERIFY(m_source->minimumUpdateInterval() > 0);
}

void TestQGeoSatelliteInfoSource::startUpdates_testIntervals()
{
    CHECK_SOURCE_VALID;
    QSignalSpy spyView(m_source,
                       SIGNAL(satellitesInViewUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy spyUse(m_source,
                      SIGNAL(satellitesInUseUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy timeout(m_source, SIGNAL(requestTimeout()));
    m_source->setUpdateInterval(7000);
    int interval = m_source->updateInterval();

    m_source->startUpdates();
    QTRY_VERIFY_WITH_TIMEOUT((spyView.count() == 1) && (spyUse.count() == 1), 9500);
    for (int i = 0; i < 6; i++) {
        QTRY_VERIFY_WITH_TIMEOUT((spyView.count() == 1) && (spyUse.count() == 1) && (timeout.count() == 0), (interval*2));
        spyView.clear();
        spyUse.clear();
    }
    m_source->stopUpdates();
}


void TestQGeoSatelliteInfoSource::startUpdates_testIntervalChangesWhileRunning()
{
    // There are two ways of dealing with an interval change, and we have left it system dependent.
    // The interval can be changed will running or after the next update.
    // WinCE uses the first method, S60 uses the second method.

    // The minimum interval on the symbian emulator is 5000 msecs, which is why the times in
    // this test are as high as they are.

    CHECK_SOURCE_VALID;
    QSignalSpy spyView(m_source,
                       SIGNAL(satellitesInViewUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy spyUse(m_source,
                      SIGNAL(satellitesInUseUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy timeout(m_source, SIGNAL(requestTimeout()));
    m_source->setUpdateInterval(0);
    m_source->startUpdates();
    m_source->setUpdateInterval(0);

    QTRY_VERIFY_WITH_TIMEOUT((spyView.count() > 0) && (spyUse.count() > 0), 7000);
    QCOMPARE(timeout.count(), 0);
    spyView.clear();
    spyUse.clear();

    m_source->setUpdateInterval(5000);

    QTRY_VERIFY_WITH_TIMEOUT((spyView.count() == 2) && (spyUse.count() == 2) && (timeout.count() == 0), 15000);
    spyView.clear();
    spyUse.clear();

    m_source->setUpdateInterval(10000);

    QTRY_VERIFY_WITH_TIMEOUT((spyView.count() == 2) && (spyUse.count() == 2) && (timeout.count() == 0), 30000);
    spyView.clear();
    spyUse.clear();

    m_source->setUpdateInterval(5000);

    QTRY_VERIFY_WITH_TIMEOUT((spyView.count() == 2) && (spyUse.count() == 2) && (timeout.count() == 0), 15000);
    spyView.clear();
    spyUse.clear();

    m_source->setUpdateInterval(5000);

    QTRY_VERIFY_WITH_TIMEOUT( (spyView.count() == 2) && (spyUse.count() == 2) && (timeout.count() == 0), 15000);
    spyView.clear();
    spyUse.clear();

    m_source->setUpdateInterval(0);

    QTRY_VERIFY_WITH_TIMEOUT( (spyView.count() > 0 ) && (spyUse.count() > 0) && (timeout.count() == 0), 7000);
    spyView.clear();
    spyUse.clear();

    m_source->setUpdateInterval(0);

    QTRY_VERIFY_WITH_TIMEOUT( (spyView.count() > 0 ) && (spyUse.count() > 0) && (timeout.count() == 0), 7000);
    spyView.clear();
    spyUse.clear();
    m_source->stopUpdates();
}

void TestQGeoSatelliteInfoSource::startUpdates_testDefaultInterval()
{
    CHECK_SOURCE_VALID;
    QSignalSpy spyView(m_source,
                       SIGNAL(satellitesInViewUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy spyUse(m_source,
                      SIGNAL(satellitesInUseUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy timeout(m_source, SIGNAL(requestTimeout()));
    m_source->startUpdates();
    for (int i = 0; i < 3; i++) {
        QTRY_VERIFY_WITH_TIMEOUT( (spyView.count() > 0 ) && (spyUse.count() > 0) && (timeout.count() == 0), 7000);
        spyView.clear();
        spyUse.clear();
    }
    m_source->stopUpdates();
}

void TestQGeoSatelliteInfoSource::startUpdates_testZeroInterval()
{
    CHECK_SOURCE_VALID;
    QSignalSpy spyView(m_source,
                       SIGNAL(satellitesInViewUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy spyUse(m_source,
                      SIGNAL(satellitesInUseUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy timeout(m_source, SIGNAL(requestTimeout()));

    m_source->setUpdateInterval(0);
    m_source->startUpdates();
    for (int i = 0; i < 3; i++) {
        QTRY_VERIFY_WITH_TIMEOUT( (spyView.count() > 0 ) && (spyUse.count() > 0) && (timeout.count() == 0), 7000);
        spyView.clear();
        spyUse.clear();
    }
    m_source->stopUpdates();
}

void TestQGeoSatelliteInfoSource::startUpdates_moreThanOnce()
{
    CHECK_SOURCE_VALID;
    QSignalSpy spyView(m_source,
                       SIGNAL(satellitesInViewUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy spyUse(m_source,
                      SIGNAL(satellitesInUseUpdated(QList<QGeoSatelliteInfo>)));
    m_source->setUpdateInterval(0);
    m_source->startUpdates();

    m_source->startUpdates(); // check there is no crash

    QTRY_VERIFY_WITH_TIMEOUT((spyView.count() > 0) && (spyUse.count() > 0), MAX_WAITING_TIME);

    m_source->startUpdates(); // check there is no crash

    m_source->stopUpdates();
}

void TestQGeoSatelliteInfoSource::stopUpdates()
{

    CHECK_SOURCE_VALID;

    QSignalSpy spyView(m_source,
                       SIGNAL(satellitesInViewUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy spyUse(m_source,
                      SIGNAL(satellitesInUseUpdated(QList<QGeoSatelliteInfo>)));
    m_source->setUpdateInterval(10000);
    m_source->startUpdates();

    for (int i = 0; i < 2; i++) {
        QTRY_VERIFY_WITH_TIMEOUT((spyView.count() == 1) && (spyUse.count() == 1), 12000);
        spyView.clear();
        spyUse.clear();
    }

    m_source->stopUpdates();

    QTRY_VERIFY_WITH_TIMEOUT((spyView.count() == 0) && (spyUse.count() == 0), 12000);
}

void TestQGeoSatelliteInfoSource::stopUpdates_withoutStart()
{
    CHECK_SOURCE_VALID;

    m_source->stopUpdates(); // check there is no crash
}

void TestQGeoSatelliteInfoSource::requestUpdate()
{
    CHECK_SOURCE_VALID;

    QFETCH(int, timeout);
    QSignalSpy spy(m_source, SIGNAL(requestTimeout()));
    QSignalSpy spyView(m_source,
                       SIGNAL(satellitesInViewUpdated(QList<QGeoSatelliteInfo>)));
    m_source->requestUpdate(timeout);
    // Geoclue may deliver update instantly if there is a satellite fix
    QTRY_VERIFY_WITH_TIMEOUT(!spy.isEmpty() || !spyView.isEmpty(), 10);
}

void TestQGeoSatelliteInfoSource::requestUpdate_data()
{
    QTest::addColumn<int>("timeout");
    QTest::newRow("less than zero") << -1;
    QTest::newRow("very small timeout") << 1;
}

void TestQGeoSatelliteInfoSource::requestUpdate_validTimeout()
{
    CHECK_SOURCE_VALID;

    QSignalSpy spyView(m_source,
                       SIGNAL(satellitesInViewUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy spyUse(m_source,
                      SIGNAL(satellitesInUseUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy spyTimeout(m_source, SIGNAL(requestTimeout()));

    m_source->requestUpdate(7000);

    QTRY_VERIFY_WITH_TIMEOUT(
            (spyView.count() == 1) && (spyUse.count() == 1 && (spyTimeout.count()) == 0), 7000);
}

void TestQGeoSatelliteInfoSource::requestUpdate_defaultTimeout()
{
    CHECK_SOURCE_VALID;

    QSignalSpy spyView(m_source,
                       SIGNAL(satellitesInViewUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy spyUse(m_source,
                      SIGNAL(satellitesInUseUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy spyTimeout(m_source, SIGNAL(requestTimeout()));

    m_source->requestUpdate(0);

    QTRY_VERIFY_WITH_TIMEOUT(
            (spyView.count() == 1) && (spyUse.count() == 1 && (spyTimeout.count()) == 0),
            MAX_WAITING_TIME);
}

void TestQGeoSatelliteInfoSource::requestUpdate_timeoutLessThanMinimumInterval()
{
    CHECK_SOURCE_VALID;

    QSignalSpy spyTimeout(m_source, SIGNAL(requestTimeout()));
    m_source->requestUpdate(1);

    QTRY_COMPARE_WITH_TIMEOUT(spyTimeout.count(), 1, 1000);
}

void TestQGeoSatelliteInfoSource::requestUpdate_repeatedCalls()
{
    CHECK_SOURCE_VALID;

    QSignalSpy spyView(m_source,
                       SIGNAL(satellitesInViewUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy spyUse(m_source,
                      SIGNAL(satellitesInUseUpdated(QList<QGeoSatelliteInfo>)));

    m_source->requestUpdate(7000);

    QTRY_VERIFY_WITH_TIMEOUT((spyView.count() == 1) && (spyUse.count() == 1), 7000);
    spyView.clear();
    spyUse.clear();

    m_source->requestUpdate(7000);

    QTRY_VERIFY_WITH_TIMEOUT((spyView.count() == 1) && (spyUse.count() == 1), 7000);
}

void TestQGeoSatelliteInfoSource::requestUpdate_overlappingCalls()
{
    CHECK_SOURCE_VALID;

    QSignalSpy spyView(m_source,
                       SIGNAL(satellitesInViewUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy spyUse(m_source,
                      SIGNAL(satellitesInUseUpdated(QList<QGeoSatelliteInfo>)));

    m_source->requestUpdate(7000);
    m_source->requestUpdate(7000);

    QTRY_VERIFY_WITH_TIMEOUT((spyView.count() == 1) && (spyUse.count() == 1), 7000);
}

void TestQGeoSatelliteInfoSource::requestUpdate_overlappingCallsWithTimeout()
{
    CHECK_SOURCE_VALID;

    QSignalSpy spyView(m_source,
                       SIGNAL(satellitesInViewUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy spyUse(m_source,
                      SIGNAL(satellitesInUseUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy spyTimeout(m_source,
                      SIGNAL(requestTimeout()));

    m_source->requestUpdate(0);
    m_source->requestUpdate(1);

    QTRY_COMPARE_WITH_TIMEOUT(spyTimeout.count(), 0, 7000);

    QTRY_VERIFY_WITH_TIMEOUT((spyView.count() == 1) && (spyUse.count() == 1), 7000);
}

void TestQGeoSatelliteInfoSource::requestUpdateAfterStartUpdates_ZeroInterval()
{
    CHECK_SOURCE_VALID;

    QSignalSpy spyView(m_source,
                       SIGNAL(satellitesInViewUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy spyUse(m_source,
                      SIGNAL(satellitesInUseUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy spyTimeout(m_source, SIGNAL(requestTimeout()));

    m_source->setUpdateInterval(0);
    m_source->startUpdates();

    QTRY_VERIFY_WITH_TIMEOUT((spyView.count() == 1) && (spyUse.count() == 1), MAX_WAITING_TIME);
    spyView.clear();
    spyUse.clear();

    m_source->requestUpdate(7000);

    QTRY_VERIFY_WITH_TIMEOUT((spyView.count() == 1) && (spyUse.count() == 1)
                             && (spyTimeout.count() == 0), 7000);

    spyView.clear();
    spyUse.clear();

    QTRY_VERIFY_WITH_TIMEOUT((spyView.count() == 1) && (spyUse.count() == 1), 12000);

    m_source->stopUpdates();
}

void TestQGeoSatelliteInfoSource::requestUpdateAfterStartUpdates_SmallInterval()
{
    CHECK_SOURCE_VALID;

    QSignalSpy spyView(m_source,
                       SIGNAL(satellitesInViewUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy spyUse(m_source,
                      SIGNAL(satellitesInUseUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy spyTimeout(m_source, SIGNAL(requestTimeout()));
    m_source->setUpdateInterval(10000);
    m_source->requestUpdate(7000);

    m_source->startUpdates();

    QTRY_VERIFY_WITH_TIMEOUT((spyView.count() > 0) && (spyUse.count() > 0)
                             && (spyTimeout.count() == 0), 7000);

    spyView.clear();
    spyUse.clear();

    QTRY_VERIFY_WITH_TIMEOUT((spyView.count() == 1) && (spyUse.count() == 1), 12000);

    m_source->stopUpdates();
}

void TestQGeoSatelliteInfoSource::requestUpdateBeforeStartUpdates_ZeroInterval()
{
    CHECK_SOURCE_VALID;
    QSignalSpy spyView(m_source,
                       SIGNAL(satellitesInViewUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy spyUse(m_source,
                      SIGNAL(satellitesInUseUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy timeout(m_source, SIGNAL(requestTimeout()));

    m_source->requestUpdate(7000);

    m_source->setUpdateInterval(0);
    m_source->startUpdates();

    QTRY_VERIFY_WITH_TIMEOUT((spyView.count() >= 2) && (spyUse.count() >= 2) && (timeout.count() == 0), 14000);
    spyView.clear();
    spyUse.clear();

    QTest::qWait(7000);

    QCOMPARE(timeout.count(), 0);

    m_source->stopUpdates();
}

void TestQGeoSatelliteInfoSource::requestUpdateBeforeStartUpdates_SmallInterval()
{
    CHECK_SOURCE_VALID;
    QSignalSpy spyView(m_source,
                       SIGNAL(satellitesInViewUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy spyUse(m_source,
                      SIGNAL(satellitesInUseUpdated(QList<QGeoSatelliteInfo>)));
    QSignalSpy timeout(m_source, SIGNAL(requestTimeout()));

    m_source->requestUpdate(7000);

    m_source->setUpdateInterval(10000);
    m_source->startUpdates();

    QTRY_VERIFY_WITH_TIMEOUT((spyView.count() > 0) && (spyUse.count() > 0) && (timeout.count() == 0), 7000);
    spyView.clear();
    spyUse.clear();

    QTRY_VERIFY_WITH_TIMEOUT((spyView.count() > 0) && (spyUse.count() > 0) && (timeout.count() == 0), 20000);

    m_source->stopUpdates();
}



void TestQGeoSatelliteInfoSource::removeSlotForRequestTimeout()
{
    CHECK_SOURCE_VALID;

    bool i = connect(m_source, SIGNAL(requestTimeout()), this, SLOT(test_slot1()));
    QVERIFY(i==true);
    i = connect(m_source, SIGNAL(requestTimeout()), this, SLOT(test_slot2()));
    QVERIFY(i==true);
    i = disconnect(m_source, SIGNAL(requestTimeout()), this, SLOT(test_slot1()));
    QVERIFY(i==true);

    m_source->requestUpdate(-1);
    QTRY_VERIFY_WITH_TIMEOUT((m_testSlot2Called == true), 1000);
}

void TestQGeoSatelliteInfoSource::removeSlotForSatellitesInUseUpdated()
{
    CHECK_SOURCE_VALID;

    bool i = connect(m_source, SIGNAL(satellitesInUseUpdated(QList<QGeoSatelliteInfo>)), this, SLOT(test_slot1()));
    QVERIFY(i == true);
    i = connect(m_source, SIGNAL(satellitesInUseUpdated(QList<QGeoSatelliteInfo>)), this, SLOT(test_slot2()));
    QVERIFY(i == true);
    i = disconnect(m_source, SIGNAL(satellitesInUseUpdated(QList<QGeoSatelliteInfo>)), this, SLOT(test_slot1()));
    QVERIFY(i == true);

    m_source->requestUpdate(7000);

    QTRY_VERIFY_WITH_TIMEOUT((m_testSlot2Called == true), 7000);
}

void TestQGeoSatelliteInfoSource::removeSlotForSatellitesInViewUpdated()
{
    CHECK_SOURCE_VALID;

    bool i = connect(m_source, SIGNAL(satellitesInViewUpdated(QList<QGeoSatelliteInfo>)), this, SLOT(test_slot1()));
    QVERIFY(i == true);
    i = connect(m_source, SIGNAL(satellitesInViewUpdated(QList<QGeoSatelliteInfo>)), this, SLOT(test_slot2()));
    QVERIFY(i == true);
    i = disconnect(m_source, SIGNAL(satellitesInViewUpdated(QList<QGeoSatelliteInfo>)), this, SLOT(test_slot1()));
    QVERIFY(i == true);

    m_source->requestUpdate(7000);

    QTRY_VERIFY_WITH_TIMEOUT((m_testSlot2Called == true), 7000);
}

#include "testqgeosatelliteinfosource.moc"
