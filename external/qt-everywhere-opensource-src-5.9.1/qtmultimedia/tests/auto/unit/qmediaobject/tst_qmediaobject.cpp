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

//TESTED_COMPONENT=src/multimedia

#include <QtTest/QtTest>

#include <QtCore/qtimer.h>

#include <QtMultimedia/qmediametadata.h>
#include <qmediaobject.h>
#include <qmediaservice.h>
#include <qmetadatareadercontrol.h>
#include <qaudioinputselectorcontrol.h>

#include "mockmediarecorderservice.h"
#include "mockmediaserviceprovider.h"
#include "mockmetadatareadercontrol.h"
#include "mockavailabilitycontrol.h"

class QtTestMediaObjectService : public QMediaService
{
    Q_OBJECT
public:
    QtTestMediaObjectService(QObject *parent = 0, MockAvailabilityControl *availability = 0)
        : QMediaService(parent)
        , availabilityControl(availability)
        , metaDataRef(0)
        , hasMetaData(true)
    {
    }

    QMediaControl *requestControl(const char *iid)
    {
        if (hasMetaData && qstrcmp(iid, QMetaDataReaderControl_iid) == 0)
            return &metaData;
        else if (qstrcmp(iid, QMediaAvailabilityControl_iid) == 0)
            return availabilityControl;
        else
            return 0;
    }

    void releaseControl(QMediaControl *)
    {
    }

    MockMetaDataReaderControl metaData;
    MockAvailabilityControl *availabilityControl;
    int metaDataRef;
    bool hasMetaData;
};

QT_USE_NAMESPACE

class tst_QMediaObject : public QObject
{
    Q_OBJECT

private slots:
    void propertyWatch();
    void notifySignals_data();
    void notifySignals();
    void notifyInterval_data();
    void notifyInterval();

    void nullMetaDataControl();
    void isMetaDataAvailable();
    void metaDataChanged();
    void metaData_data();
    void metaData();
    void availability();

    void service();

private:
    void setupNotifyTests();
};

class QtTestMediaObject : public QMediaObject
{
    Q_OBJECT
    Q_PROPERTY(int a READ a WRITE setA NOTIFY aChanged)
    Q_PROPERTY(int b READ b WRITE setB NOTIFY bChanged)
    Q_PROPERTY(int c READ c WRITE setC NOTIFY cChanged)
    Q_PROPERTY(int d READ d WRITE setD)
public:
    QtTestMediaObject(QMediaService *service = 0): QMediaObject(0, service), m_a(0), m_b(0), m_c(0), m_d(0) {}

    using QMediaObject::addPropertyWatch;
    using QMediaObject::removePropertyWatch;

    int a() const { return m_a; }
    void setA(int a) { m_a = a; }

    int b() const { return m_b; }
    void setB(int b) { m_b = b; }

    int c() const { return m_c; }
    void setC(int c) { m_c = c; }

    int d() const { return m_d; }
    void setD(int d) { m_d = d; }

Q_SIGNALS:
    void aChanged(int a);
    void bChanged(int b);
    void cChanged(int c);

private:
    int m_a;
    int m_b;
    int m_c;
    int m_d;
};

void tst_QMediaObject::propertyWatch()
{
    QtTestMediaObject object;
    object.setNotifyInterval(0);

    QEventLoop loop;
    connect(&object, SIGNAL(aChanged(int)), &QTestEventLoop::instance(), SLOT(exitLoop()));
    connect(&object, SIGNAL(bChanged(int)), &QTestEventLoop::instance(), SLOT(exitLoop()));
    connect(&object, SIGNAL(cChanged(int)), &QTestEventLoop::instance(), SLOT(exitLoop()));

    QSignalSpy aSpy(&object, SIGNAL(aChanged(int)));
    QSignalSpy bSpy(&object, SIGNAL(bChanged(int)));
    QSignalSpy cSpy(&object, SIGNAL(cChanged(int)));

    QTestEventLoop::instance().enterLoop(1);

    QCOMPARE(aSpy.count(), 0);
    QCOMPARE(bSpy.count(), 0);
    QCOMPARE(cSpy.count(), 0);

    int aCount = 0;
    int bCount = 0;
    int cCount = 0;

    object.addPropertyWatch("a");

    QTestEventLoop::instance().enterLoop(1);

    QVERIFY(aSpy.count() > aCount);
    QCOMPARE(bSpy.count(), 0);
    QCOMPARE(cSpy.count(), 0);
    QCOMPARE(aSpy.last().value(0).toInt(), 0);

    aCount = aSpy.count();

    object.setA(54);
    object.setB(342);
    object.setC(233);

    QTestEventLoop::instance().enterLoop(1);

    QVERIFY(aSpy.count() > aCount);
    QCOMPARE(bSpy.count(), 0);
    QCOMPARE(cSpy.count(), 0);
    QCOMPARE(aSpy.last().value(0).toInt(), 54);

    aCount = aSpy.count();

    object.addPropertyWatch("b");
    object.addPropertyWatch("d");
    object.removePropertyWatch("e");
    object.setA(43);
    object.setB(235);
    object.setC(90);

    QTestEventLoop::instance().enterLoop(1);

    QVERIFY(aSpy.count() > aCount);
    QVERIFY(bSpy.count() > bCount);
    QCOMPARE(cSpy.count(), 0);
    QCOMPARE(aSpy.last().value(0).toInt(), 43);
    QCOMPARE(bSpy.last().value(0).toInt(), 235);

    aCount = aSpy.count();
    bCount = bSpy.count();

    object.removePropertyWatch("a");
    object.addPropertyWatch("c");
    object.addPropertyWatch("e");

    QTestEventLoop::instance().enterLoop(1);

    QCOMPARE(aSpy.count(), aCount);
    QVERIFY(bSpy.count() > bCount);
    QVERIFY(cSpy.count() > cCount);
    QCOMPARE(bSpy.last().value(0).toInt(), 235);
    QCOMPARE(cSpy.last().value(0).toInt(), 90);

    bCount = bSpy.count();
    cCount = cSpy.count();

    object.setA(435);
    object.setC(9845);

    QTestEventLoop::instance().enterLoop(1);

    QCOMPARE(aSpy.count(), aCount);
    QVERIFY(bSpy.count() > bCount);
    QVERIFY(cSpy.count() > cCount);
    QCOMPARE(bSpy.last().value(0).toInt(), 235);
    QCOMPARE(cSpy.last().value(0).toInt(), 9845);

    bCount = bSpy.count();
    cCount = cSpy.count();

    object.setA(8432);
    object.setB(324);
    object.setC(443);
    object.removePropertyWatch("c");
    object.removePropertyWatch("d");

    QTestEventLoop::instance().enterLoop(1);

    QCOMPARE(aSpy.count(), aCount);
    QVERIFY(bSpy.count() > bCount);
    QCOMPARE(cSpy.count(), cCount);
    QCOMPARE(bSpy.last().value(0).toInt(), 324);
    QCOMPARE(cSpy.last().value(0).toInt(), 9845);

    bCount = bSpy.count();

    object.removePropertyWatch("b");

    QTestEventLoop::instance().enterLoop(1);

    QCOMPARE(aSpy.count(), aCount);
    QCOMPARE(bSpy.count(), bCount);
    QCOMPARE(cSpy.count(), cCount);
}

void tst_QMediaObject::setupNotifyTests()
{
    QTest::addColumn<int>("interval");
    QTest::addColumn<int>("count");

    QTest::newRow("single 750ms")
            << 750
            << 1;
    QTest::newRow("single 600ms")
            << 600
            << 1;
    QTest::newRow("x3 300ms")
            << 300
            << 3;
    QTest::newRow("x5 180ms")
            << 180
            << 5;
}

void tst_QMediaObject::notifySignals_data()
{
    setupNotifyTests();
}

void tst_QMediaObject::notifySignals()
{
    QFETCH(int, interval);
    QFETCH(int, count);

    QtTestMediaObject object;
    QSignalSpy spy(&object, SIGNAL(aChanged(int)));

    object.setNotifyInterval(interval);
    object.addPropertyWatch("a");

    QElapsedTimer timer;
    timer.start();

    QTRY_COMPARE(spy.count(), count);

    qint64 elapsed = timer.elapsed();
    int expectedElapsed = count * interval * 1.5; // give it some margin of error
    QVERIFY2(elapsed < expectedElapsed, QString("elapsed: %1, expected: %2").arg(elapsed).arg(expectedElapsed).toLocal8Bit().constData());
}

void tst_QMediaObject::notifyInterval_data()
{
    setupNotifyTests();
}

void tst_QMediaObject::notifyInterval()
{
    QFETCH(int, interval);

    QtTestMediaObject object;
    QSignalSpy spy(&object, SIGNAL(notifyIntervalChanged(int)));

    object.setNotifyInterval(interval);
    QCOMPARE(object.notifyInterval(), interval);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().value(0).toInt(), interval);

    object.setNotifyInterval(interval);
    QCOMPARE(object.notifyInterval(), interval);
    QCOMPARE(spy.count(), 1);
}

void tst_QMediaObject::nullMetaDataControl()
{
    const QString titleKey(QLatin1String("Title"));
    const QString title(QLatin1String("Host of Seraphim"));

    QtTestMediaObjectService service;
    service.hasMetaData = false;

    QtTestMediaObject object(&service);

    QSignalSpy spy(&object, SIGNAL(metaDataChanged()));

    QCOMPARE(object.isMetaDataAvailable(), false);

    QCOMPARE(object.metaData(QMediaMetaData::Title).toString(), QString());
    QCOMPARE(object.availableMetaData(), QStringList());
    QCOMPARE(spy.count(), 0);
}

void tst_QMediaObject::isMetaDataAvailable()
{
    QtTestMediaObjectService service;
    service.metaData.setMetaDataAvailable(false);

    QtTestMediaObject object(&service);
    QCOMPARE(object.isMetaDataAvailable(), false);

    QSignalSpy spy(&object, SIGNAL(metaDataAvailableChanged(bool)));
    service.metaData.setMetaDataAvailable(true);

    QCOMPARE(object.isMetaDataAvailable(), true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);

    service.metaData.setMetaDataAvailable(false);

    QCOMPARE(object.isMetaDataAvailable(), false);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toBool(), false);
}

void tst_QMediaObject::metaDataChanged()
{
    QtTestMediaObjectService service;
    QtTestMediaObject object(&service);

    QSignalSpy changedSpy(&object, SIGNAL(metaDataChanged()));
    QSignalSpy changedWithValueSpy(&object, SIGNAL(metaDataChanged(QString,QVariant)));

    service.metaData.setMetaData("key", "Value");
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(changedWithValueSpy.count(), 1);
    QCOMPARE(changedWithValueSpy.last()[0], QVariant("key"));
    QCOMPARE(changedWithValueSpy.last()[1].value<QVariant>(), QVariant("Value"));

    service.metaData.setMetaData("key", "Value");
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(changedWithValueSpy.count(), 1);

    service.metaData.setMetaData("key2", "Value");
    QCOMPARE(changedSpy.count(), 2);
    QCOMPARE(changedWithValueSpy.count(), 2);
    QCOMPARE(changedWithValueSpy.last()[0], QVariant("key2"));
    QCOMPARE(changedWithValueSpy.last()[1].value<QVariant>(), QVariant("Value"));
}

void tst_QMediaObject::metaData_data()
{
    QTest::addColumn<QString>("artist");
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("genre");

    QTest::newRow("")
            << QString::fromLatin1("Dead Can Dance")
            << QString::fromLatin1("Host of Seraphim")
            << QString::fromLatin1("Awesome");
}

void tst_QMediaObject::metaData()
{
    QFETCH(QString, artist);
    QFETCH(QString, title);
    QFETCH(QString, genre);

    QtTestMediaObjectService service;
    service.metaData.populateMetaData();

    QtTestMediaObject object(&service);
    QVERIFY(object.availableMetaData().isEmpty());

    service.metaData.m_data.insert(QMediaMetaData::AlbumArtist, artist);
    service.metaData.m_data.insert(QMediaMetaData::Title, title);
    service.metaData.m_data.insert(QMediaMetaData::Genre, genre);

    QCOMPARE(object.metaData(QMediaMetaData::AlbumArtist).toString(), artist);
    QCOMPARE(object.metaData(QMediaMetaData::Title).toString(), title);

    QStringList metaDataKeys = object.availableMetaData();
    QCOMPARE(metaDataKeys.size(), 3);
    QVERIFY(metaDataKeys.contains(QMediaMetaData::AlbumArtist));
    QVERIFY(metaDataKeys.contains(QMediaMetaData::Title));
    QVERIFY(metaDataKeys.contains(QMediaMetaData::Genre));
}

void tst_QMediaObject::availability()
{
    {
        QtTestMediaObject nullObject(0);
        QCOMPARE(nullObject.isAvailable(), false);
        QCOMPARE(nullObject.availability(), QMultimedia::ServiceMissing);
    }

    {
        QtTestMediaObjectService service;
        QtTestMediaObject object(&service);
        QCOMPARE(object.isAvailable(), true);
        QCOMPARE(object.availability(), QMultimedia::Available);
    }

    {
        MockAvailabilityControl available(QMultimedia::Available);
        QtTestMediaObjectService service(0, &available);
        QtTestMediaObject object(&service);
        QSignalSpy availabilitySpy(&object, SIGNAL(availabilityChanged(bool)));
        QSignalSpy availabilityStatusSpy(&object, SIGNAL(availabilityChanged(QMultimedia::AvailabilityStatus)));

        QCOMPARE(object.isAvailable(), true);
        QCOMPARE(object.availability(), QMultimedia::Available);

        available.setAvailability(QMultimedia::Busy);
        QCOMPARE(object.isAvailable(), false);
        QCOMPARE(object.availability(), QMultimedia::Busy);
        QCOMPARE(availabilitySpy.count(), 1);
        QCOMPARE(availabilityStatusSpy.count(), 1);

        available.setAvailability(QMultimedia::Available);
        QCOMPARE(object.isAvailable(), true);
        QCOMPARE(object.availability(), QMultimedia::Available);
        QCOMPARE(availabilitySpy.count(), 2);
        QCOMPARE(availabilityStatusSpy.count(), 2);
    }

    {
        MockAvailabilityControl available(QMultimedia::Busy);
        QtTestMediaObjectService service(0, &available);
        QtTestMediaObject object(&service);
        QSignalSpy availabilitySpy(&object, SIGNAL(availabilityChanged(bool)));
        QSignalSpy availabilityStatusSpy(&object, SIGNAL(availabilityChanged(QMultimedia::AvailabilityStatus)));

        QCOMPARE(object.isAvailable(), false);
        QCOMPARE(object.availability(), QMultimedia::Busy);

        available.setAvailability(QMultimedia::Available);
        QCOMPARE(object.isAvailable(), true);
        QCOMPARE(object.availability(), QMultimedia::Available);
        QCOMPARE(availabilitySpy.count(), 1);
        QCOMPARE(availabilityStatusSpy.count(), 1);
    }
}

 void tst_QMediaObject::service()
 {
     // Create the mediaobject with service.
     QtTestMediaObjectService service;
     QtTestMediaObject mediaObject1(&service);

     // Get service and Compare if it equal to the service passed as an argument in mediaObject1.
     QMediaService *service1 = mediaObject1.service();
     QVERIFY(service1 != NULL);
     QCOMPARE(service1,&service);

     // Create the mediaobject with empty service and verify that service() returns NULL.
     QtTestMediaObject mediaObject2;
     QMediaService *service2 = mediaObject2.service();
     QVERIFY(service2 == NULL);
}

QTEST_GUILESS_MAIN(tst_QMediaObject)
#include "tst_qmediaobject.moc"
