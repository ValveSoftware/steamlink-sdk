/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#include <QString>
#include <QtTest>
#include <QtSensors/QSensorGestureManager>
#include <QtSensors/QSensorGesture>
#include <QFile>

#include "mockbackends.h"

class tst_sensorgestures_gestures : public QObject
{
    Q_OBJECT

public:
    tst_sensorgestures_gestures();

private Q_SLOTS:
    void initTestCase();

    void testTiltedTwist();
    void testNotPickup();

    void testNotHover2();
    void testNotHover();
    void testNotWhip();

    void testSingleGestures();
    void testSingleGestures_data();

    void testSingleDataset2Gestures();
    void testSingleDataset2Gestures_data();

    void testTwist();
    void testTwist_data();

    void testShake2();
    void testShake2_data();

    void testShake();

    void testAllGestures();
    void testAllGestures_data();


protected:
    mockSensorPlugin plugin;

};

tst_sensorgestures_gestures::tst_sensorgestures_gestures()
{
}

void tst_sensorgestures_gestures::initTestCase()
{
    qputenv("QT_SENSORS_LOAD_PLUGINS", "0"); // Do not load plugins
    plugin.registerSensors();
}

void tst_sensorgestures_gestures::testSingleGestures()
{
    QFETCH(QString, gestureId);

    QString name = "mock_data/sensordata_" + gestureId + ".dat";

    QSensorGestureManager manager;
    QStringList idList = manager.gestureIds();
    QString gestStr = QLatin1String("QtSensors.") + gestureId;

    QVERIFY(idList.contains(gestStr));

    QScopedPointer<QSensorGesture> gesture(new QSensorGesture(QStringList() << gestStr));
    QVERIFY(gesture.data()->validIds().contains(gestStr));

    QSignalSpy spy_gesture(gesture.data(), SIGNAL(detected(QString)));

    QCOMPARE(mockcommonPrivate::instance()->setFile(name), true);
    gesture.data()->startDetection();
    QCOMPARE(gesture->isActive(),true);

    QTRY_COMPARE_WITH_TIMEOUT(spy_gesture.count(),1, 7000);
}

void tst_sensorgestures_gestures::testSingleGestures_data()
{
    QTest::addColumn<QString>("gestureId");
    QTest::newRow("cover") << "cover";
    QTest::newRow("doubletap") << "doubletap";
    QTest::newRow("hover") << "hover";
    QTest::newRow("pickup") << "pickup";
    QTest::newRow("shake2") << "shake2"; //multi?
    QTest::newRow("slam") << "slam";
    QTest::newRow("turnover") << "turnover";
    QTest::newRow("twist") << "twist"; //multi?
    QTest::newRow("whip") << "whip";
}

void tst_sensorgestures_gestures::testSingleDataset2Gestures()
{
    QFETCH(QString, gestureId);

    QString name = "dataset2_mock_data/sensordata_" + gestureId + ".dat";
    QSensorGestureManager manager;
    QStringList idList = manager.gestureIds();

    QString gestStr = QLatin1String("QtSensors.") + gestureId;

    QVERIFY(idList.contains(gestStr));

    QScopedPointer<QSensorGesture> gesture(new QSensorGesture(QStringList() << gestStr));
    QVERIFY(gesture.data()->validIds().contains(gestStr));

    QSignalSpy spy_gesture(gesture.data(), SIGNAL(detected(QString)));

    QCOMPARE(mockcommonPrivate::instance()->setFile(name), true);
    gesture.data()->startDetection();
    QCOMPARE(gesture->isActive(),true);

    QTRY_COMPARE_WITH_TIMEOUT(spy_gesture.count(),1, 7000);
}

void tst_sensorgestures_gestures::testSingleDataset2Gestures_data()
{
    QTest::addColumn<QString>("gestureId");
    QTest::newRow("cover") << "cover";
    QTest::newRow("doubletap") << "doubletap";
    QTest::newRow("hover") << "hover";
    QTest::newRow("pickup") << "pickup";
    QTest::newRow("shake2") << "shake2"; //multi?
    QTest::newRow("slam") << "slam";
    QTest::newRow("turnover") << "turnover";
    QTest::newRow("twist") << "twist"; //multi?
    QTest::newRow("whip") << "whip";
}

void tst_sensorgestures_gestures::testTwist()
{
    QFETCH(QString, gestureSignal);

    QString name = "mock_data/sensordata_" + gestureSignal + ".dat";

    QSensorGestureManager manager;
    QStringList idList = manager.gestureIds();

    QString gestStr = QLatin1String("QtSensors.twist");

    QVERIFY(idList.contains(gestStr));

    QScopedPointer<QSensorGesture> gesture2(new QSensorGesture(QStringList() << gestStr));
    QVERIFY(gesture2.data()->validIds().contains(gestStr));

    QSignalSpy spy_gesture(gesture2.data(), SIGNAL(detected(QString)));

    QCOMPARE(mockcommonPrivate::instance()->setFile(name), true);
    gesture2.data()->startDetection();
    QCOMPARE(gesture2->isActive(),true);

    QTRY_COMPARE_WITH_TIMEOUT(spy_gesture.count(),1, 7000);

    QList<QVariant> arguments = spy_gesture.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QString(gestureSignal));
}

void tst_sensorgestures_gestures::testTwist_data()
{
    QTest::addColumn<QString>("gestureSignal");
    QTest::newRow("twistLeft") << "twistLeft";
    QTest::newRow("twistRight") << "twistRight";
}

void tst_sensorgestures_gestures::testShake2()
{
    QFETCH(QString, gestureSignal);

    QString name = "mock_data/sensordata_" + gestureSignal + ".dat";

    QSensorGestureManager manager;
    QStringList idList = manager.gestureIds();

    QString gestStr = QLatin1String("QtSensors.shake2");

    QVERIFY(idList.contains(gestStr));

    QScopedPointer<QSensorGesture> gesture(new QSensorGesture(QStringList() << gestStr));
    QVERIFY(gesture.data()->validIds().contains(gestStr));

    QSignalSpy spy_gesture(gesture.data(), SIGNAL(detected(QString)));

    QCOMPARE(mockcommonPrivate::instance()->setFile(name), true);
    gesture.data()->startDetection();
    QCOMPARE(gesture->isActive(),true);

    QTRY_COMPARE_WITH_TIMEOUT(spy_gesture.count(),1, 7000);
    QList<QVariant> arguments = spy_gesture.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QString(gestureSignal));

    gesture.data()->stopDetection();
}

void tst_sensorgestures_gestures::testShake2_data()
{
    QTest::addColumn<QString>("gestureSignal");
    QTest::newRow("shakeLeft") << "shakeLeft";
    QTest::newRow("shakeRight") << "shakeRight";
    QTest::newRow("shakeUp") << "shakeUp";
    QTest::newRow("shakeDown") << "shakeDown";
}

void tst_sensorgestures_gestures::testShake()
{
    QString gestureSignal = "shake";

    QString name = "mock_data/sensordata_shake2.dat";

    QSensorGestureManager manager;
    QStringList idList = manager.gestureIds();

    QString gestStr = QLatin1String("QtSensors.shake");

    QVERIFY(idList.contains(gestStr));

    QScopedPointer<QSensorGesture> gesture(new QSensorGesture(QStringList() << gestStr));
    QVERIFY(gesture.data()->validIds().contains(gestStr));

    QSignalSpy spy_gesture(gesture.data(), SIGNAL(detected(QString)));

    QCOMPARE(mockcommonPrivate::instance()->setFile(name), true);
    gesture.data()->startDetection();
    QCOMPARE(gesture->isActive(),true);

    QTRY_COMPARE_WITH_TIMEOUT(spy_gesture.count(),1, 7000);
    QList<QVariant> arguments = spy_gesture.takeFirst();

    QCOMPARE(arguments.at(0).toString(), QString(gestureSignal));
    gesture.data()->stopDetection();
}


void tst_sensorgestures_gestures::testAllGestures_data()
{
    testSingleGestures_data();
}

void tst_sensorgestures_gestures::testAllGestures()
{
    QFETCH(QString, gestureId);

    QString name = "dataset2_mock_data/sensordata_" + gestureId + ".dat";

    QSensorGestureManager manager;
    QStringList idList = manager.gestureIds();

    QStringList gestStringList;

    gestStringList << "QtSensors.cover"
                   << "QtSensors.doubletap"
                   << "QtSensors.hover"
                   << "QtSensors.pickup"
                   << "QtSensors.shake2"
                   << "QtSensors.slam"
                   << "QtSensors.turnover"
                   << "QtSensors.twist"
                   << "QtSensors.whip";

    QScopedPointer<QSensorGesture> gesture(new QSensorGesture(gestStringList));

    QCOMPARE(gesture->invalidIds().count(),0);
    QSignalSpy spy_gesture(gesture.data(), SIGNAL(detected(QString)));

    QCOMPARE(mockcommonPrivate::instance()->setFile(name), true);
    gesture.data()->startDetection();
    QCOMPARE(gesture->isActive(),true);

    QTRY_COMPARE_WITH_TIMEOUT(spy_gesture.count(),1, 7000);

    gesture.data()->stopDetection();

    QList<QVariant> arguments = spy_gesture.takeFirst();
    QString gestureSignal;
    if (gestureId.right(1) == QLatin1String("2")) {
        gestureSignal = "shakeLeft";
    } else if (gestureId.contains("twist")) {
            gestureSignal = "twistLeft";
        } else {
            gestureSignal = gestureId;
        }

    QCOMPARE(arguments.at(0).toString(), QString(gestureSignal));
}

void tst_sensorgestures_gestures::testNotHover()
{
    QString name = "mock_data/sensordata_nothover.dat";

    QSensorGestureManager manager;
    QStringList idList = manager.gestureIds();

    QStringList gestStringList;

    gestStringList << "QtSensors.hover";
    QScopedPointer<QSensorGesture> gesture(new QSensorGesture(gestStringList));

    QCOMPARE(gesture->invalidIds().count(),0);
    QSignalSpy spy_gesture(gesture.data(), SIGNAL(detected(QString)));

    QCOMPARE(mockcommonPrivate::instance()->setFile(name), true);
    gesture.data()->startDetection();
    QCOMPARE(gesture->isActive(),true);

    QTRY_COMPARE_WITH_TIMEOUT(spy_gesture.count(),0, 2000);

}

void tst_sensorgestures_gestures::testNotWhip()
{

    QString name = "mock_data/sensordata_notwhip.dat";

    QSensorGestureManager manager;
    QStringList idList = manager.gestureIds();

    QStringList gestStringList;

    gestStringList << "QtSensors.whip";
    QScopedPointer<QSensorGesture> gesture(new QSensorGesture(gestStringList));

    QCOMPARE(gesture->invalidIds().count(),0);
    QSignalSpy spy_gesture(gesture.data(), SIGNAL(detected(QString)));

    QCOMPARE(mockcommonPrivate::instance()->setFile(name), true);
    gesture.data()->startDetection();
    QCOMPARE(gesture->isActive(),true);

    QTRY_COMPARE_WITH_TIMEOUT(spy_gesture.count(),0, 2000);

}

void tst_sensorgestures_gestures::testNotHover2()
{
// test slam when coming to close to head
    QString name = "dataset2_mock_data/sensordata_nothover2.dat";

    QStringList gestStringList;

    gestStringList << "QtSensors.hover";
    gestStringList << "QtSensors.slam";

    QScopedPointer<QSensorGesture> gesture(new QSensorGesture(gestStringList));

    QCOMPARE(gesture->invalidIds().count(),0);
    QSignalSpy spy_gesture(gesture.data(), SIGNAL(detected(QString)));

    QCOMPARE(mockcommonPrivate::instance()->setFile(name), true);
    gesture.data()->startDetection();
    QCOMPARE(gesture->isActive(),true);

    QTRY_COMPARE_WITH_TIMEOUT(spy_gesture.count(),1, 2000);

    QList<QVariant> arguments = spy_gesture.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QLatin1String("slam"));
}

void tst_sensorgestures_gestures::testTiltedTwist()
{
    QString name = "mock_data/sensordata_tiltedtwist.dat";

    QStringList gestStringList;

    gestStringList << "QtSensors.twist";
    QScopedPointer<QSensorGesture> gesture(new QSensorGesture(gestStringList));

    QCOMPARE(gesture->invalidIds().count(),0);
    QSignalSpy spy_gesture(gesture.data(), SIGNAL(detected(QString)));

    QCOMPARE(mockcommonPrivate::instance()->setFile(name), true);
    gesture.data()->startDetection();
    QCOMPARE(gesture->isActive(),true);

    QTRY_COMPARE_WITH_TIMEOUT(spy_gesture.count(),1, 7000);

    QList<QVariant> arguments = spy_gesture.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QLatin1String("twistLeft"));
}

void tst_sensorgestures_gestures::testNotPickup()
{
//    QString name = "mock_data/sensordata_notpickup.dat";

//    QStringList gestStringList;
//    gestStringList << "QtSensors.pickup" << "QtSensors.twist";

//    QScopedPointer<QSensorGesture> gesture(new QSensorGesture(gestStringList));

//    QCOMPARE(gesture->invalidIds().count(),0);
//    QSignalSpy spy_gesture(gesture.data(), SIGNAL(detected(QString)));

//    QCOMPARE(mockcommonPrivate::instance()->setFile(name), true);
//    gesture.data()->startDetection();
//    QCOMPARE(gesture->isActive(),true);

//    QTRY_COMPARE_WITH_TIMEOUT(spy_gesture.count(),1, 7000);

//    QList<QVariant> arguments = spy_gesture.takeFirst();
//    QCOMPARE(arguments.at(0).toString(), QLatin1String("twistLeft"));
}



QTEST_MAIN(tst_sensorgestures_gestures)

#include "tst_sensorgestures_gestures.moc"
