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

#include <limits.h>
#include <float.h>

#include <QDebug>
#include <QDataStream>

#include <QtPositioning/qgeoareamonitorinfo.h>
#include <QtPositioning/qgeoareamonitorsource.h>
#include <QtPositioning/qgeopositioninfo.h>
#include <QtPositioning/qgeopositioninfosource.h>
#include <QtPositioning/qnmeapositioninfosource.h>
#include <QtPositioning/qgeocircle.h>
#include <QtPositioning/qgeorectangle.h>

#include "logfilepositionsource.h"


QT_USE_NAMESPACE
#define UPDATE_INTERVAL 200

Q_DECLARE_METATYPE(QGeoPositionInfo)
Q_DECLARE_METATYPE(QGeoAreaMonitorInfo)

QString tst_qgeoareamonitorinfo_debug;

void tst_qgeoareamonitorinfo_messageHandler(QtMsgType type,
                                            const QMessageLogContext &,
                                            const QString &msg)
{
    switch (type) {
        case QtDebugMsg :
            tst_qgeoareamonitorinfo_debug = msg;
            break;
        default:
            break;
    }
}

class tst_QGeoAreaMonitorSource : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        /*
         * Set custom path since CI doesn't install plugins
         */
#ifdef Q_OS_WIN
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath() +
                                     QStringLiteral("/../../../../plugins"));
#else
        QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath()
                                         + QStringLiteral("/../../../plugins"));
#endif
        qRegisterMetaType<QGeoPositionInfo>();
        qRegisterMetaType<QGeoAreaMonitorInfo>();
    }

    void init()
    {
    }

    void cleanup()
    {
        QGeoAreaMonitorSource *obj = QGeoAreaMonitorSource::createSource(QStringLiteral("positionpoll"), 0);
        QVERIFY(obj != 0);
        QCOMPARE(obj->sourceName(), QStringLiteral("positionpoll"));

        QList<QGeoAreaMonitorInfo> list = obj->activeMonitors();
        if (list.count() > 0) {
            //cleanup installed monitors
            foreach (const QGeoAreaMonitorInfo& info, list) {
                QVERIFY(obj->stopMonitoring(info));
            }
        }
        QVERIFY(obj->activeMonitors().count() == 0);
    }

    void cleanupTestCase()
    {
    }

    void tst_monitor()
    {
        QGeoAreaMonitorInfo defaultMonitor;
        QVERIFY(defaultMonitor.name().isEmpty());
        QVERIFY(!defaultMonitor.identifier().isEmpty());
        QCOMPARE(defaultMonitor.isPersistent(), false);
        QVERIFY(!defaultMonitor.area().isValid());
        QVERIFY(!defaultMonitor.isValid());
        QCOMPARE(defaultMonitor.expiration(), QDateTime());
        QCOMPARE(defaultMonitor.notificationParameters(), QVariantMap());

        QGeoAreaMonitorSource *obj = QGeoAreaMonitorSource::createSource(QStringLiteral("positionpoll"), 0);
        QVERIFY(obj != 0);
        QCOMPARE(obj->sourceName(), QStringLiteral("positionpoll"));
        QVERIFY(!obj->startMonitoring(defaultMonitor));
        QCOMPARE(obj->activeMonitors().count(), 0);
        QVERIFY(!obj->requestUpdate(defaultMonitor,
                                    SIGNAL(areaEntered(QGeoMonitorInfo,QGeoAreaPositionInfo))));
        delete obj;

        //copy constructor based
        QGeoAreaMonitorInfo copy(defaultMonitor);
        QVERIFY(copy.name().isEmpty());
        QCOMPARE(copy.identifier(), defaultMonitor.identifier());
        QVERIFY(copy == defaultMonitor);
        QVERIFY(!(copy != defaultMonitor));
        QCOMPARE(copy.isPersistent(), false);

        copy.setName(QString("my name"));
        QCOMPARE(copy.name(), QString("my name"));


        QDateTime now = QDateTime::currentDateTime().addSecs(1000); //little bit in the future
        copy.setExpiration(now);
        QVERIFY(copy != defaultMonitor);
        QCOMPARE(copy.expiration(), now);

        QCOMPARE(copy.isPersistent(), defaultMonitor.isPersistent());
        copy.setPersistent(true);
        QCOMPARE(copy.isPersistent(), true);
        QCOMPARE(defaultMonitor.isPersistent(), false);
        copy.setPersistent(false);

        QVERIFY(copy.area() == defaultMonitor.area());
        QVERIFY(!copy.area().isValid());
        copy.setArea(QGeoCircle(QGeoCoordinate(1, 2), 4));
        QVERIFY(copy.area().isValid());
        QVERIFY(copy.area() != defaultMonitor.area());
        QVERIFY(copy.area().contains(QGeoCoordinate(1, 2)));

        QVERIFY(copy.notificationParameters().isEmpty());
        QVariantMap map;
        map.insert(QString("MyKey"), QVariant(123));
        copy.setNotificationParameters(map);
        QVERIFY(!copy.notificationParameters().isEmpty());
        QCOMPARE(copy.notificationParameters().value(QString("MyKey")).toInt(), 123);
        QCOMPARE(defaultMonitor.notificationParameters().value(QString("MyKey")).toInt(), 0);

        QCOMPARE(defaultMonitor.identifier(), copy.identifier());

        //assignment operator based
        QGeoAreaMonitorInfo assignmentCopy;
        assignmentCopy = copy;
        QVERIFY(copy == assignmentCopy);
        QVERIFY(assignmentCopy != defaultMonitor);

        QVERIFY(assignmentCopy.area().contains(QGeoCoordinate(1, 2)));
        QCOMPARE(assignmentCopy.expiration(), now);
        QCOMPARE(assignmentCopy.isPersistent(), false);
        QCOMPARE(assignmentCopy.notificationParameters().value(QString("MyKey")).toInt(), 123);
        QCOMPARE(defaultMonitor.identifier(), assignmentCopy.identifier());
        QCOMPARE(assignmentCopy.name(), QString("my name"));

        //validity checks for requestUpdate()
        obj = QGeoAreaMonitorSource::createSource(QStringLiteral("positionpoll"), 0);
        QVERIFY(obj != 0);
        QCOMPARE(obj->sourceName(), QStringLiteral("positionpoll"));
        QCOMPARE(obj->activeMonitors().count(), 0);
        //reference -> should work
        QVERIFY(obj->requestUpdate(copy, SIGNAL(areaEntered(QGeoAreaMonitorInfo,QGeoPositionInfo))));
        QCOMPARE(obj->activeMonitors().count(), 1);
        //replaces areaEntered single shot
        QVERIFY(obj->requestUpdate(copy, SIGNAL(areaExited(QGeoAreaMonitorInfo,QGeoPositionInfo))));
        QCOMPARE(obj->activeMonitors().count(), 1);
        //replaces areaExited single shot
        QVERIFY(obj->startMonitoring(copy));
        QCOMPARE(obj->activeMonitors().count(), 1);


        //invalid signal
        QVERIFY(!obj->requestUpdate(copy, 0));
        QCOMPARE(obj->activeMonitors().count(), 1);

        //signal that doesn't exist
        QVERIFY(!obj->requestUpdate(copy, SIGNAL(areaEntered(QGeoMonitor))));
        QCOMPARE(obj->activeMonitors().count(), 1);

        QVERIFY(!obj->requestUpdate(copy, "SIGNAL(areaEntered(QGeoMonitor))"));
        QCOMPARE(obj->activeMonitors().count(), 1);

        //ensure that we cannot add a persistent monitor to a source
        //that doesn't support persistence
        QGeoAreaMonitorInfo persistenceMonitor(copy);
        persistenceMonitor.setPersistent(obj->supportedAreaMonitorFeatures() & QGeoAreaMonitorSource::PersistentAreaMonitorFeature);
        persistenceMonitor.setPersistent(!persistenceMonitor.isPersistent());

        QVERIFY(!obj->requestUpdate(persistenceMonitor, SIGNAL(areaEntered(QGeoAreaMonitorInfo,QGeoPositionInfo))));
        QCOMPARE(obj->activeMonitors().count(), 1);
        QVERIFY(!obj->startMonitoring(persistenceMonitor));
        QCOMPARE(obj->activeMonitors().count(), 1);

        //ensure that persistence was only reason for rejection
        persistenceMonitor.setPersistent(!persistenceMonitor.isPersistent());
        QVERIFY(obj->startMonitoring(persistenceMonitor));
        //persistenceMonitor is copy of already added monitor
        //the last call was an update
        QCOMPARE(obj->activeMonitors().count(), 1);

        delete obj;
    }

    void tst_monitorValid()
    {
        QGeoAreaMonitorInfo mon;
        QVERIFY(!mon.isValid());
        QCOMPARE(mon.name(), QString());
        QCOMPARE(mon.area().isValid(), false);

        QGeoAreaMonitorInfo mon2 = mon;
        QVERIFY(!mon2.isValid());

        QGeoShape invalidShape;
        QGeoCircle emptyCircle(QGeoCoordinate(0,1), 0);
        QGeoCircle validCircle(QGeoCoordinate(0,1), 1);

        //all invalid since no name set yet
        mon2.setArea(invalidShape);
        QVERIFY(mon2.area() == invalidShape);
        QVERIFY(!mon2.isValid());

        mon2.setArea(emptyCircle);
        QVERIFY(mon2.area() == emptyCircle);
        QVERIFY(!mon2.isValid());

        mon2.setArea(validCircle);
        QVERIFY(mon2.area() == validCircle);
        QVERIFY(!mon2.isValid());

        //valid since name and non-empy shape has been set
        QGeoAreaMonitorInfo validMonitor("TestMonitor");
        QVERIFY(validMonitor.name() == QString("TestMonitor"));
        QVERIFY(!validMonitor.isValid());

        validMonitor.setArea(invalidShape);
        QVERIFY(validMonitor.area() == invalidShape);
        QVERIFY(!validMonitor.isValid());

        validMonitor.setArea(emptyCircle);
        QVERIFY(validMonitor.area() == emptyCircle);
        QVERIFY(!validMonitor.isValid());

        validMonitor.setArea(validCircle);
        QVERIFY(validCircle == validMonitor.area());
        QVERIFY(validMonitor.isValid());
    }

    void tst_monitorStreaming()
    {
        QByteArray container;
        QDataStream stream(&container, QIODevice::ReadWrite);

        QGeoAreaMonitorInfo monitor("someName");
        monitor.setArea(QGeoCircle(QGeoCoordinate(1,3), 5.4));
        QVERIFY(monitor.isValid());
        QCOMPARE(monitor.name(), QString("someName"));

        QGeoAreaMonitorInfo target;
        QVERIFY(!target.isValid());
        QVERIFY(target.name().isEmpty());

        QVERIFY(target != monitor);

        stream << monitor;
        stream.device()->seek(0);
        stream >> target;

        QVERIFY(target == monitor);
        QVERIFY(target.isValid());
        QCOMPARE(target.name(), QString("someName"));
        QVERIFY(target.area() == QGeoCircle(QGeoCoordinate(1,3), 5.4));
    }

    void tst_createDefaultSource()
    {
        QObject* parent = new QObject;
        QGeoAreaMonitorSource* obj = QGeoAreaMonitorSource::createDefaultSource(parent);
        QVERIFY(obj != 0);
        QVERIFY(obj->parent() == parent);
        delete obj;

        const QStringList monitors = QGeoAreaMonitorSource::availableSources();
        QVERIFY(!monitors.isEmpty());
        QVERIFY(monitors.contains(QStringLiteral("positionpoll")));

        obj = QGeoAreaMonitorSource::createSource(QStringLiteral("positionpoll"), parent);
        QVERIFY(obj != 0);
        QCOMPARE(obj->sourceName(), QStringLiteral("positionpoll"));
        delete parent;

        obj = QGeoAreaMonitorSource::createSource(QStringLiteral("randomNonExistingName"), 0);
        QVERIFY(obj == 0);
    }

    void tst_activeMonitors()
    {
        QGeoAreaMonitorSource *obj = QGeoAreaMonitorSource::createSource(QStringLiteral("positionpoll"), 0);
        QVERIFY(obj != 0);
        QCOMPARE(obj->sourceName(), QStringLiteral("positionpoll"));

        LogFilePositionSource *source = new LogFilePositionSource(this);
        source->setUpdateInterval(UPDATE_INTERVAL);
        obj->setPositionInfoSource(source);
        QCOMPARE(obj->positionInfoSource(), source);


        QVERIFY(obj->activeMonitors().isEmpty());

        QGeoAreaMonitorInfo mon("Monitor_Circle");
        mon.setArea(QGeoCircle(QGeoCoordinate(1,1), 1000));
        QVERIFY(obj->startMonitoring(mon));

        QGeoAreaMonitorInfo mon2("Monitor_rectangle_below");
        QGeoRectangle r_below(QGeoCoordinate(1,1),2,2);
        mon2.setArea(r_below);
        QVERIFY(obj->startMonitoring(mon2));

        QGeoAreaMonitorInfo mon3("Monitor_rectangle_above");
        QGeoRectangle r_above(QGeoCoordinate(2,1),2,2);
        mon3.setArea(r_above);
        QVERIFY(obj->startMonitoring(mon3));

        QList<QGeoAreaMonitorInfo> results = obj->activeMonitors();
        QCOMPARE(results.count(), 3);
        foreach (const QGeoAreaMonitorInfo& info, results) {
            QVERIFY(info == mon || info == mon2 || info == mon3);
        }

        results = obj->activeMonitors(QGeoShape());
        QCOMPARE(results.count(), 0);

        results = obj->activeMonitors(QGeoRectangle(QGeoCoordinate(1,1),0.2, 0.2));
        QCOMPARE(results.count(), 2);
        foreach (const QGeoAreaMonitorInfo& info, results) {
            QVERIFY(info == mon || info == mon2);
        }

        results = obj->activeMonitors(QGeoCircle(QGeoCoordinate(1,1),1000));
        QCOMPARE(results.count(), 2);
        foreach (const QGeoAreaMonitorInfo& info, results) {
            QVERIFY(info == mon || info == mon2);
        }

        results = obj->activeMonitors(QGeoCircle(QGeoCoordinate(2,1),1000));
        QCOMPARE(results.count(), 1);
        foreach (const QGeoAreaMonitorInfo& info, results) {
            QVERIFY(info == mon3);
        }

        //same as above except that we use a different monitor source object instance
        //all monitor objects of same type share same active monitors
        QGeoAreaMonitorSource *secondObj = QGeoAreaMonitorSource::createSource(QStringLiteral("positionpoll"), 0);
        QVERIFY(secondObj != 0);
        QCOMPARE(secondObj->sourceName(), QStringLiteral("positionpoll"));

        results = secondObj->activeMonitors();
        QCOMPARE(results.count(), 3);
        foreach (const QGeoAreaMonitorInfo& info, results) {
            QVERIFY(info == mon || info == mon2 || info == mon3);
        }

        results = secondObj->activeMonitors(QGeoShape());
        QCOMPARE(results.count(), 0);

        results = secondObj->activeMonitors(QGeoRectangle(QGeoCoordinate(1,1),0.2, 0.2));
        QCOMPARE(results.count(), 2);
        foreach (const QGeoAreaMonitorInfo& info, results) {
            QVERIFY(info == mon || info == mon2);
        }

        results = secondObj->activeMonitors(QGeoCircle(QGeoCoordinate(1,1),1000));
        QCOMPARE(results.count(), 2);
        foreach (const QGeoAreaMonitorInfo& info, results) {
            QVERIFY(info == mon || info == mon2);
        }

        results = secondObj->activeMonitors(QGeoCircle(QGeoCoordinate(2,1),1000));
        QCOMPARE(results.count(), 1);
        foreach (const QGeoAreaMonitorInfo& info, results) {
            QVERIFY(info == mon3);
        }

        delete obj;
        delete secondObj;
    }

    void tst_testExpiryTimeout()
    {
        QGeoAreaMonitorSource *obj = QGeoAreaMonitorSource::createSource(QStringLiteral("positionpoll"), 0);
        QVERIFY(obj != 0);
        QCOMPARE(obj->sourceName(), QStringLiteral("positionpoll"));

        QGeoAreaMonitorSource *secondObj = QGeoAreaMonitorSource::createSource(QStringLiteral("positionpoll"), 0);
        QVERIFY(secondObj != 0);
        QCOMPARE(secondObj->sourceName(), QStringLiteral("positionpoll"));

        LogFilePositionSource *source = new LogFilePositionSource(this);
        source->setUpdateInterval(UPDATE_INTERVAL);
        obj->setPositionInfoSource(source);

        //Singleton pattern behind QGeoAreaMonitorSource ensures same position info source
        QCOMPARE(obj->positionInfoSource(), source);
        QCOMPARE(secondObj->positionInfoSource(), source);

        QSignalSpy expirySpy(obj, SIGNAL(monitorExpired(QGeoAreaMonitorInfo)));
        QSignalSpy expirySpy2(secondObj, SIGNAL(monitorExpired(QGeoAreaMonitorInfo)));

        QDateTime now = QDateTime::currentDateTime();

        const int monitorCount = 4;
        for (int i = 1; i <= monitorCount; i++) {
            QGeoAreaMonitorInfo mon(QString::number(i));
            mon.setArea(QGeoRectangle(QGeoCoordinate(i,i), i, i));
            mon.setExpiration(now.addSecs(i*5));
            QVERIFY(mon.isValid());
            QVERIFY(obj->startMonitoring(mon));
        }



        QCOMPARE(obj->activeMonitors().count(), monitorCount);
        QCOMPARE(secondObj->activeMonitors().count(), monitorCount);

        QGeoAreaMonitorInfo info("InvalidExpiry");
        info.setArea(QGeoRectangle(QGeoCoordinate(10,10), 1, 1 ));
        QVERIFY(info.isValid());
        info.setExpiration(now.addSecs(-1000));
        QVERIFY(info.expiration() < now);
        QVERIFY(!obj->startMonitoring(info));
        QCOMPARE(obj->activeMonitors().count(), monitorCount);
        QVERIFY(!obj->requestUpdate(info, SIGNAL(areaEntered(QGeoAreaMonitorInfo,QGeoPositionInfo))));
        QCOMPARE(obj->activeMonitors().count(), monitorCount);

        for (int i = 1; i <= monitorCount; i++) {
            QTRY_VERIFY_WITH_TIMEOUT(expirySpy.count() == 1, 7000); //each expiry within 5 s
            QGeoAreaMonitorInfo mon = expirySpy.takeFirst().at(0).value<QGeoAreaMonitorInfo>();
            QCOMPARE(obj->activeMonitors().count(), monitorCount-i);
            QCOMPARE(mon.name(), QString::number(i));
        }

        QCOMPARE(expirySpy2.count(), monitorCount);
        QCOMPARE(secondObj->activeMonitors().count(), 0); //all monitors expired
        for (int i = 1; i <= monitorCount; i++) {
            QGeoAreaMonitorInfo mon = expirySpy2.takeFirst().at(0).value<QGeoAreaMonitorInfo>();
            QCOMPARE(mon.name(), QString::number(i));
        }

        delete obj;
        delete secondObj;
    }

    void tst_enteredExitedSignal()
    {
        QGeoAreaMonitorSource *obj = QGeoAreaMonitorSource::createSource(QStringLiteral("positionpoll"), 0);
        QVERIFY(obj != 0);
        QCOMPARE(obj->sourceName(), QStringLiteral("positionpoll"));
        obj->setObjectName("firstObject");
        QSignalSpy enteredSpy(obj, SIGNAL(areaEntered(QGeoAreaMonitorInfo,QGeoPositionInfo)));
        QSignalSpy exitedSpy(obj, SIGNAL(areaExited(QGeoAreaMonitorInfo,QGeoPositionInfo)));

        LogFilePositionSource *source = new LogFilePositionSource(this);
        source->setUpdateInterval(UPDATE_INTERVAL);
        obj->setPositionInfoSource(source);
        QCOMPARE(obj->positionInfoSource(), source);

        QGeoAreaMonitorSource *secondObj = QGeoAreaMonitorSource::createSource(QStringLiteral("positionpoll"), 0);
        QVERIFY(secondObj != 0);
        QCOMPARE(secondObj->sourceName(), QStringLiteral("positionpoll"));
        QSignalSpy enteredSpy2(secondObj, SIGNAL(areaEntered(QGeoAreaMonitorInfo,QGeoPositionInfo)));
        QSignalSpy exitedSpy2(secondObj, SIGNAL(areaExited(QGeoAreaMonitorInfo,QGeoPositionInfo)));
        secondObj->setObjectName("secondObject");

        QGeoAreaMonitorInfo infoRectangle("Rectangle");
        infoRectangle.setArea(QGeoRectangle(QGeoCoordinate(-27.65, 153.093), 0.2, 0.2));
        QVERIFY(infoRectangle.isValid());
        QVERIFY(obj->startMonitoring(infoRectangle));

        QGeoAreaMonitorInfo infoCircle("Circle");
        infoCircle.setArea(QGeoCircle(QGeoCoordinate(-27.70, 153.093),10000));
        QVERIFY(infoCircle.isValid());
        QVERIFY(obj->startMonitoring(infoCircle));

        QGeoAreaMonitorInfo singleShot_enter("SingleShot_on_Entered");
        singleShot_enter.setArea(QGeoRectangle(QGeoCoordinate(-27.67, 153.093), 0.2, 0.2));
        QVERIFY(singleShot_enter.isValid());
        QVERIFY(obj->requestUpdate(singleShot_enter,
                                   SIGNAL(areaEntered(QGeoAreaMonitorInfo,QGeoPositionInfo))));

        QGeoAreaMonitorInfo singleShot_exit("SingleShot_on_Exited");
        singleShot_exit.setArea(QGeoRectangle(QGeoCoordinate(-27.70, 153.093), 0.2, 0.2));
        QVERIFY(singleShot_exit.isValid());
        QVERIFY(obj->requestUpdate(singleShot_exit,
                                   SIGNAL(areaExited(QGeoAreaMonitorInfo,QGeoPositionInfo))));

        QVERIFY(obj->activeMonitors().count() == 4); //all monitors active
        QVERIFY(secondObj->activeMonitors().count() == 4); //all monitors active

        static const int Number_Of_Entered_Events = 6;
        static const int Number_Of_Exited_Events = 5;
        //takes 87 (lines)*200(timeout)/1000 seconds to finish
        QTRY_VERIFY_WITH_TIMEOUT(enteredSpy.count() == Number_Of_Entered_Events, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(exitedSpy.count() == Number_Of_Exited_Events, 20000);
        QCOMPARE(enteredSpy.count(), Number_Of_Entered_Events);
        QCOMPARE(exitedSpy.count(), Number_Of_Exited_Events);

        QList<QGeoAreaMonitorInfo> monitorsInExpectedEnteredEventOrder;
        monitorsInExpectedEnteredEventOrder << infoRectangle << singleShot_enter << singleShot_exit
                                            << infoCircle << infoCircle << infoRectangle;

        QList<QGeoAreaMonitorInfo> monitorsInExpectedExitedEventOrder;
        monitorsInExpectedExitedEventOrder << infoRectangle << infoCircle
                                            << singleShot_exit << infoCircle << infoRectangle;

        QList<QGeoCoordinate> enteredEventCoordinateOrder;
        enteredEventCoordinateOrder << QGeoCoordinate(-27.55, 153.090718) //infoRectangle
                                    << QGeoCoordinate(-27.57, 153.090718) //singleshot_enter
                                    << QGeoCoordinate(-27.60, 153.090908) //singleshot_exit
                                    << QGeoCoordinate(-27.62, 153.091036) //infoCircle
                                    << QGeoCoordinate(-27.78, 153.093647) //infoCircle
                                    << QGeoCoordinate(-27.75, 153.093896);//infoRectangle
        QCOMPARE(enteredEventCoordinateOrder.count(), Number_Of_Entered_Events);
        QCOMPARE(monitorsInExpectedEnteredEventOrder.count(), Number_Of_Entered_Events);

        QList<QGeoCoordinate> exitedEventCoordinateOrder;
        exitedEventCoordinateOrder  << QGeoCoordinate(-27.78, 153.092218) //infoRectangle
                                    << QGeoCoordinate(-27.79, 153.092308) //infoCircle
                                    << QGeoCoordinate(-27.81, 153.092530) //singleshot_exit
                                    << QGeoCoordinate(-27.61, 153.095231) //infoCircle
                                    << QGeoCoordinate(-27.54, 153.095995);//infoCircle
        QCOMPARE(exitedEventCoordinateOrder.count(), Number_Of_Exited_Events);
        QCOMPARE(monitorsInExpectedExitedEventOrder.count(), Number_Of_Exited_Events);

        //verify that both sources got the same signals
        for (int i = 0; i < Number_Of_Entered_Events; i++) {
            //first source
            QGeoAreaMonitorInfo monInfo = enteredSpy.first().at(0).value<QGeoAreaMonitorInfo>();
            QGeoPositionInfo posInfo = enteredSpy.takeFirst().at(1).value<QGeoPositionInfo>();
            QVERIFY2(monInfo == monitorsInExpectedEnteredEventOrder.at(i),
                     qPrintable(QString::number(i) + ": " + monInfo.name()));
            QVERIFY2(posInfo.coordinate() == enteredEventCoordinateOrder.at(i),
                     qPrintable(QString::number(i) + ". posInfo"));

            //reset info objects to avoid comparing the same
            monInfo = QGeoAreaMonitorInfo();
            posInfo = QGeoPositionInfo();

            //second source
            monInfo = enteredSpy2.first().at(0).value<QGeoAreaMonitorInfo>();
            posInfo = enteredSpy2.takeFirst().at(1).value<QGeoPositionInfo>();
            QVERIFY2(monInfo == monitorsInExpectedEnteredEventOrder.at(i),
                     qPrintable(QString::number(i) + ": " + monInfo.name()));
            QVERIFY2(posInfo.coordinate() == enteredEventCoordinateOrder.at(i),
                     qPrintable(QString::number(i) + ". posInfo"));
        }

        for (int i = 0; i < Number_Of_Exited_Events; i++) {
            //first source
            QGeoAreaMonitorInfo monInfo = exitedSpy.first().at(0).value<QGeoAreaMonitorInfo>();
            QGeoPositionInfo posInfo = exitedSpy.takeFirst().at(1).value<QGeoPositionInfo>();
            QVERIFY2(monInfo == monitorsInExpectedExitedEventOrder.at(i),
                     qPrintable(QString::number(i) + ": " + monInfo.name()));
            QVERIFY2(posInfo.coordinate() == exitedEventCoordinateOrder.at(i),
                     qPrintable(QString::number(i) + ". posInfo"));

            //reset info objects to avoid comparing the same
            monInfo = QGeoAreaMonitorInfo();
            posInfo = QGeoPositionInfo();

            //second source
            monInfo = exitedSpy2.first().at(0).value<QGeoAreaMonitorInfo>();
            posInfo = exitedSpy2.takeFirst().at(1).value<QGeoPositionInfo>();
            QVERIFY2(monInfo == monitorsInExpectedExitedEventOrder.at(i),
                     qPrintable(QString::number(i) + ": " + monInfo.name()));
            QVERIFY2(posInfo.coordinate() == exitedEventCoordinateOrder.at(i),
                     qPrintable(QString::number(i) + ". posInfo"));
        }

        QCOMPARE(obj->activeMonitors().count(), 2); //single shot monitors have been removed
        QCOMPARE(secondObj->activeMonitors().count(), 2);

        delete obj;
        delete secondObj;
    }

    void tst_swapOfPositionSource()
    {
        QGeoAreaMonitorSource *obj = QGeoAreaMonitorSource::createSource(QStringLiteral("positionpoll"), 0);
        QVERIFY(obj != 0);
        QCOMPARE(obj->sourceName(), QStringLiteral("positionpoll"));
        obj->setObjectName("firstObject");
        QSignalSpy enteredSpy(obj, SIGNAL(areaEntered(QGeoAreaMonitorInfo,QGeoPositionInfo)));
        QSignalSpy exitedSpy(obj, SIGNAL(areaExited(QGeoAreaMonitorInfo,QGeoPositionInfo)));

        QGeoAreaMonitorSource *obj2 = QGeoAreaMonitorSource::createSource(QStringLiteral("positionpoll"), 0);
        QVERIFY(obj2 != 0);
        QCOMPARE(obj2->sourceName(), QStringLiteral("positionpoll"));
        obj2->setObjectName("secondObject");
        QSignalSpy enteredSpy2(obj2, SIGNAL(areaEntered(QGeoAreaMonitorInfo,QGeoPositionInfo)));
        QSignalSpy exitedSpy2(obj2, SIGNAL(areaExited(QGeoAreaMonitorInfo,QGeoPositionInfo)));

        LogFilePositionSource *source = new LogFilePositionSource(this);
        source->setUpdateInterval(UPDATE_INTERVAL);
        source->setObjectName("FirstLogFileSource");

        LogFilePositionSource *source2 = new LogFilePositionSource(this);
        source2->setUpdateInterval(UPDATE_INTERVAL);
        source2->setObjectName("SecondLogFileSource");

        obj->setPositionInfoSource(source);
        QCOMPARE(obj->positionInfoSource(), obj2->positionInfoSource());
        QCOMPARE(obj2->positionInfoSource(), source);

        QGeoAreaMonitorInfo infoRectangle("Rectangle");
        infoRectangle.setArea(QGeoRectangle(QGeoCoordinate(-27.70, 153.092), 0.2, 0.2));
        QVERIFY(infoRectangle.isValid());
        QVERIFY(obj->startMonitoring(infoRectangle));

        QCOMPARE(obj->activeMonitors().count(), 1);
        QCOMPARE(obj2->activeMonitors().count(), 1);

        QGeoCoordinate firstBorder(-27.6, 153.090908);
        QGeoCoordinate secondBorder(-27.81, 153.092530);

        /***********************************/
        //1. trigger events on source (until areaExit
        QTRY_VERIFY_WITH_TIMEOUT(exitedSpy.count() == 1, 20000);
        QCOMPARE(enteredSpy.count(), enteredSpy2.count());
        QCOMPARE(exitedSpy.count(), exitedSpy2.count());

        //compare entered event
        QVERIFY(enteredSpy.first().at(0).value<QGeoAreaMonitorInfo>() ==
                enteredSpy2.first().at(0).value<QGeoAreaMonitorInfo>());
        QGeoPositionInfo info = enteredSpy.takeFirst().at(1).value<QGeoPositionInfo>();
        QVERIFY(info == enteredSpy2.takeFirst().at(1).value<QGeoPositionInfo>());
        QVERIFY(info.coordinate() == firstBorder);
        //compare exit event
        QVERIFY(exitedSpy.first().at(0).value<QGeoAreaMonitorInfo>() ==
                exitedSpy2.first().at(0).value<QGeoAreaMonitorInfo>());
        info = exitedSpy.takeFirst().at(1).value<QGeoPositionInfo>();
        QVERIFY(info == exitedSpy2.takeFirst().at(1).value<QGeoPositionInfo>());
        QVERIFY(info.coordinate() == secondBorder);

        QCOMPARE(exitedSpy.count(), 0);
        QCOMPARE(enteredSpy.count(), 0);
        QCOMPARE(exitedSpy2.count(), 0);
        QCOMPARE(enteredSpy2.count(), 0);

        /***********************************/
        //2. change position source -> which restarts at beginning again
        obj2->setPositionInfoSource(source2);
        QCOMPARE(obj->positionInfoSource(), obj2->positionInfoSource());
        QCOMPARE(obj2->positionInfoSource(), source2);

        QTRY_VERIFY_WITH_TIMEOUT(exitedSpy.count() == 1, 20000);
        QCOMPARE(enteredSpy.count(), enteredSpy2.count());
        QCOMPARE(exitedSpy.count(), exitedSpy2.count());

        //compare entered event
        QVERIFY(enteredSpy.first().at(0).value<QGeoAreaMonitorInfo>() ==
                enteredSpy2.first().at(0).value<QGeoAreaMonitorInfo>());
        info = enteredSpy.takeFirst().at(1).value<QGeoPositionInfo>();
        QVERIFY(info == enteredSpy2.takeFirst().at(1).value<QGeoPositionInfo>());
        QVERIFY(info.coordinate() == firstBorder);
        //compare exit event
        QVERIFY(exitedSpy.first().at(0).value<QGeoAreaMonitorInfo>() ==
                exitedSpy2.first().at(0).value<QGeoAreaMonitorInfo>());
        info = exitedSpy.takeFirst().at(1).value<QGeoPositionInfo>();
        QVERIFY(info == exitedSpy2.takeFirst().at(1).value<QGeoPositionInfo>());
        QVERIFY(info.coordinate() == secondBorder);


        //obj was deleted when setting new source
        delete obj2;
    }

    void debug_data()
    {
        QTest::addColumn<QGeoAreaMonitorInfo>("info");
        QTest::addColumn<int>("nextValue");
        QTest::addColumn<QString>("debugString");

        QGeoAreaMonitorInfo info;
        QTest::newRow("uninitialized") << info << 45
                << QString("QGeoAreaMonitorInfo(\"\", QGeoShape(Unknown), "
                              "persistent: false, expiry: QDateTime( Qt::TimeSpec(LocalTime))) 45");

        info.setArea(QGeoRectangle());
        info.setPersistent(true);
        info.setName("RectangleAreaMonitor");
        QTest::newRow("Rectangle Test") << info  << 45
                << QString("QGeoAreaMonitorInfo(\"RectangleAreaMonitor\", QGeoShape(Rectangle), "
                              "persistent: true, expiry: QDateTime( Qt::TimeSpec(LocalTime))) 45");

        info = QGeoAreaMonitorInfo();
        info.setArea(QGeoCircle());
        info.setPersistent(false);
        info.setName("CircleAreaMonitor");
        QVariantMap map;
        map.insert(QString("foobarKey"), QVariant(45)); //should be ignored
        info.setNotificationParameters(map);
        QTest::newRow("Circle Test") << info  << 45
                << QString("QGeoAreaMonitorInfo(\"CircleAreaMonitor\", QGeoShape(Circle), "
                              "persistent: false, expiry: QDateTime( Qt::TimeSpec(LocalTime))) 45");

        // we ignore any further QDateTime related changes to avoid depending on QDateTime related
        // failures in case its QDebug string changes
    }

    void debug()
    {
        QFETCH(QGeoAreaMonitorInfo, info);
        QFETCH(int, nextValue);
        QFETCH(QString, debugString);

        qInstallMessageHandler(tst_qgeoareamonitorinfo_messageHandler);
        qDebug() << info << nextValue;
        qInstallMessageHandler(0);
        QCOMPARE(tst_qgeoareamonitorinfo_debug, debugString);
    }
};


QTEST_GUILESS_MAIN(tst_QGeoAreaMonitorSource)
#include "tst_qgeoareamonitor.moc"
