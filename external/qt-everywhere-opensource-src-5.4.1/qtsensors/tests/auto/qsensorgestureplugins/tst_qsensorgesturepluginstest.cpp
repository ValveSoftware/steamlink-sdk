/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#include <QtCore/QString>
#include <QtTest/QtTest>

#include <QVariant>
#include <QSignalSpy>

#include <qsensorgesture.h>
#include <qsensorgesturemanager.h>

#include <qsensorgesturerecognizer.h>
#include <qsensorgestureplugininterface.h>

#include "test_backends.h"

class Tst_qsensorgesturePluginsTest : public QObject
{
    Q_OBJECT

public:
    Tst_qsensorgesturePluginsTest();

private Q_SLOTS:

    void tst_sensor_plugins_shake();
    void tst_sensor_plugins_qtsensors_data();
    void tst_sensor_plugins_qtsensors();
    void tst_sensor_plugins_qtsensors_all();

};

Tst_qsensorgesturePluginsTest::Tst_qsensorgesturePluginsTest()
{
    qputenv("QT_SENSORS_LOAD_PLUGINS", "0"); // Do not load plugins
    register_test_backends();
}

void Tst_qsensorgesturePluginsTest::tst_sensor_plugins_shake()
{

    QSensorGestureManager manager;
    QVERIFY(manager.gestureIds().contains("QtSensors.shake"));

    QSensorGestureRecognizer *recognizer = manager.sensorGestureRecognizer("QtSensors.shake");

    QCOMPARE(recognizer->isActive(), false);

    QTest::ignoreMessage(QtWarningMsg, "Not starting. Gesture Recognizer not initialized");
    recognizer->startBackend();
    QCOMPARE(recognizer->isActive(), false);

    QTest::ignoreMessage(QtWarningMsg, "Not stopping. Gesture Recognizer not initialized");
    recognizer->stopBackend();
    QCOMPARE(recognizer->isActive(), false);

    QScopedPointer<QSensorGesture> gesture(new QSensorGesture(QStringList() << "QtSensors.shake"));

    QCOMPARE(gesture->isActive(),false);
    QCOMPARE(gesture->validIds(), QStringList() << "QtSensors.shake");

    QCOMPARE(QStringList() << recognizer->id(), gesture->validIds());

    gesture->startDetection();
    QCOMPARE(gesture->isActive(),true);
    QCOMPARE(recognizer->isActive(), true);

    gesture->stopDetection();
    QCOMPARE(recognizer->isActive(), false);
}

void Tst_qsensorgesturePluginsTest::tst_sensor_plugins_qtsensors_data()
{
    QTest::addColumn<QString>("gestureId");

    QTest::newRow("QtSensors.cover") << "QtSensors.cover";

    QTest::newRow("QtSensors.doubletap") << "QtSensors.doubletap";

    QTest::newRow("QtSensors.freefall") << "QtSensors.freefall";
    QTest::newRow("QtSensors.hover") << "QtSensors.hover";
    QTest::newRow("QtSensors.shake2") << "QtSensors.shake2";
    QTest::newRow("QtSensors.slam") << "QtSensors.slam";
    QTest::newRow("QtSensors.turnover") << "QtSensors.turnover";
    QTest::newRow("QtSensors.twist") << "QtSensors.twist";
    QTest::newRow("QtSensors.whip") << "QtSensors.whip";
}

void Tst_qsensorgesturePluginsTest::tst_sensor_plugins_qtsensors()
{
    QFETCH(QString, gestureId);
    QSensorGestureManager manager;

    QVERIFY(manager.gestureIds().contains(gestureId));

    QScopedPointer<QSensorGesture> gesture(new QSensorGesture(QStringList() << gestureId));
    QScopedPointer<QSensorGesture> gesture2(new QSensorGesture(QStringList() << gestureId));

    QCOMPARE(gesture->isActive(),false);
    QCOMPARE(gesture->validIds(), QStringList() << gestureId);

    QSensorGestureRecognizer *recognizer = manager.sensorGestureRecognizer(gestureId);

    QCOMPARE(QStringList() << recognizer->id(), gesture->validIds());

    QCOMPARE(QStringList() << QStringList(), gesture->invalidIds());

    QVERIFY(recognizer->isActive() == false);
    QVERIFY(gesture->isActive() == false);
    QVERIFY(gesture2->isActive() == false);

    gesture->startDetection();

    QVERIFY(gesture->isActive() == true);
    QVERIFY(gesture2->isActive() == false);

    QVERIFY(recognizer->isActive() == true);

    gesture2->startDetection();

    QVERIFY(gesture->isActive() == true);
    QVERIFY(gesture2->isActive() == true);
    QVERIFY(recognizer->isActive() == true);

    gesture2->stopDetection();

    QVERIFY(gesture->isActive() == true);
    QVERIFY(gesture2->isActive() == false);
    QVERIFY(recognizer->isActive() == true);

    gesture->stopDetection();

    QVERIFY(gesture->isActive() == false);
    QVERIFY(gesture2->isActive() == false);
    QVERIFY(recognizer->isActive() == false);


    QVERIFY(recognizer->gestureSignals().count() > 1);
    QVERIFY(recognizer->gestureSignals().contains("detected(QString)"));

}

void Tst_qsensorgesturePluginsTest::tst_sensor_plugins_qtsensors_all()
{
    QStringList list;
    list << "QtSensors.cover";
    list << "QtSensors.doubletap";
    list << "QtSensors.hover";
    list << "QtSensors.pickup";
    list << "QtSensors.shake2";
    list << "QtSensors.slam";
    list << "QtSensors.turnover";
    list << "QtSensors.twist";
    list << "QtSensors.whip";

    QSensorGestureManager manager;

    QScopedPointer<QSensorGesture> gesture(new QSensorGesture(list));
    QVERIFY(gesture->invalidIds().count() == 0);
    QVERIFY(gesture->validIds().count() == 9);
    QVERIFY(gesture->gestureSignals().count() == 14);

    gesture->startDetection();
    QVERIFY(gesture->isActive());

    QSensorGestureRecognizer *recognizer = manager.sensorGestureRecognizer("QtSensors.turnover");
    recognizer->stopBackend();
    QVERIFY(!recognizer->isActive());
    QVERIFY(gesture->isActive());

}


QTEST_MAIN(Tst_qsensorgesturePluginsTest);

#include "tst_qsensorgesturepluginstest.moc"
