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

#include <QtTest/QtTest>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QSensor>
#include "test_backends.h"

class tst_legacy_sensors : public QObject
{
    Q_OBJECT
public:
    tst_legacy_sensors(QObject *parent = 0)
        : QObject(parent)
    {
        qputenv("QT_SENSORS_LOAD_PLUGINS", "0"); // Do not load plugins
        register_test_backends();
    }

private slots:
    void initTestCase()
    {
    }

    void cleanupTestCase()
    {
    }

    void versions_data()
    {
        QTest::addColumn<QString>("version");
        QTest::addColumn<bool>("exists");

        QTest::newRow("5.0") << "5.0" << true;
    }

    void versions()
    {
        QFETCH(QString, version);
        QFETCH(bool, exists);

        QQmlEngine engine;
        QString qml = QString("import QtQuick 2.0\nimport QtSensors %1\nItem {}").arg(version);
        QQmlComponent c(&engine);
        c.setData(qml.toLocal8Bit(), QUrl::fromLocalFile(QDir::currentPath()));
        if (!exists)
            QTest::ignoreMessage(QtWarningMsg, "QQmlComponent: Component is not ready");
        QObject *obj = c.create();
        QCOMPARE(exists, (obj != 0));
        delete obj;
        QList<QQmlError> errors = c.errors();
        if (exists) {
            QCOMPARE(errors.count(), 0);
        } else {
            QCOMPARE(errors.count(), 1);
            QString expected = QString("module \"QtSensors\" version %1 is not installed").arg(version);
            QString actual = errors.first().description();
            QCOMPARE(expected, actual);
        }
    }

    void elements_data()
    {
        QTest::addColumn<QString>("version");
        QTest::addColumn<QString>("element");
        QTest::addColumn<bool>("exists");

        QTest::newRow("5.0 Range") << "5.0" << "Range" << false;
        QTest::newRow("5.0 OutputRange") << "5.0" << "OutputRange" << false;
        QTest::newRow("5.0 Sensor") << "5.0" << "Sensor" << false;
        QTest::newRow("5.0 SensorReading") << "5.0" << "SensorReading" << false;
        QTest::newRow("5.0 Accelerometer") << "5.0" << "Accelerometer" << true;
        QTest::newRow("5.0 AccelerometerReading") << "5.0" << "AccelerometerReading" << false;
        QTest::newRow("5.0 AmbientLightSensor") << "5.0" << "AmbientLightSensor" << true;
        QTest::newRow("5.0 AmbientLightReading") << "5.0" << "AmbientLightReading" << false;
        QTest::newRow("5.0 Compass") << "5.0" << "Compass" << true;
        QTest::newRow("5.0 CompassReading") << "5.0" << "CompassReading" << false;
        QTest::newRow("5.0 Magnetometer") << "5.0" << "Magnetometer" << true;
        QTest::newRow("5.0 MagnetometerReading") << "5.0" << "MagnetometerReading" << false;
        QTest::newRow("5.0 OrientationSensor") << "5.0" << "OrientationSensor" << true;
        QTest::newRow("5.0 OrientationReading") << "5.0" << "OrientationReading" << false;
        QTest::newRow("5.0 ProximitySensor") << "5.0" << "ProximitySensor" << true;
        QTest::newRow("5.0 ProximityReading") << "5.0" << "ProximityReading" << false;
        QTest::newRow("5.0 RotationSensor") << "5.0" << "RotationSensor" << true;
        QTest::newRow("5.0 RotationReading") << "5.0" << "RotationReading" << false;
        QTest::newRow("5.0 TapSensor") << "5.0" << "TapSensor" << true;
        QTest::newRow("5.0 TapReading") << "5.0" << "TapReading" << false;
        QTest::newRow("5.0 LightSensor") << "5.0" << "LightSensor" << true;
        QTest::newRow("5.0 LightReading") << "5.0" << "LightReading" << false;
        QTest::newRow("5.0 Gyroscope") << "5.0" << "Gyroscope" << true;
        QTest::newRow("5.0 GyroscopeReading") << "5.0" << "GyroscopeReading" << false;
        QTest::newRow("5.0 IRProximitySensor") << "5.0" << "IRProximitySensor" << true;
        QTest::newRow("5.0 IRProximityReading") << "5.0" << "IRProximityReading" << false;
        QTest::newRow("5.0 TiltSensor") << "5.0" << "TiltSensor" << true;
        QTest::newRow("5.0 TiltReading") << "5.0" << "TiltReading" << false;

        QTest::newRow("5.0 SensorGesture") << "5.0" << "SensorGesture" << true;
    }

    void elements()
    {
        QFETCH(QString, version);
        QFETCH(QString, element);
        QFETCH(bool, exists);

        QQmlEngine engine;
        QString qml = QString("import QtQuick 2.0\nimport QtSensors %1\n%2 {}").arg(version).arg(element);
        QQmlComponent c(&engine);
        c.setData(qml.toLocal8Bit(), QUrl::fromLocalFile(QDir::currentPath()));
        if (!exists)
            QTest::ignoreMessage(QtWarningMsg, "QQmlComponent: Component is not ready");
        QObject *obj = c.create();
        QCOMPARE(exists, (obj != 0));
        delete obj;
        QList<QQmlError> errors = c.errors();
        if (exists) {
            QCOMPARE(errors.count(), 0);
        } else {
            QCOMPARE(errors.count(), 1);
            QString expected = QString("Cannot create %1").arg(element);
            QString actual = errors.first().description();
            QCOMPARE(expected, actual);
        }
    }

    void alwaysOn_data()
    {
        QTest::addColumn<QString>("version");
        QTest::addColumn<QString>("element");
        QTest::addColumn<bool>("validSyntax");

        QTest::newRow("5.0 Accelerometer") << "5.0" << "Accelerometer" << true;
        QTest::newRow("5.0 AmbientLightSensor") << "5.0" << "AmbientLightSensor" << true;
        QTest::newRow("5.0 Compass") << "5.0" << "Compass" << true;
        QTest::newRow("5.0 Magnetometer") << "5.0" << "Magnetometer" << true;
        QTest::newRow("5.0 OrientationSensor") << "5.0" << "OrientationSensor" << true;
        QTest::newRow("5.0 ProximitySensor") << "5.0" << "ProximitySensor" << true;
        QTest::newRow("5.0 RotationSensor") << "5.0" << "RotationSensor" << true;
        QTest::newRow("5.0 TapSensor") << "5.0" << "TapSensor" << true;
        QTest::newRow("5.0 LightSensor") << "5.0" << "LightSensor" << true;
        QTest::newRow("5.0 Gyroscope") << "5.0" << "Gyroscope" << true;
        QTest::newRow("5.0 IRProximitySensor") << "5.0" << "IRProximitySensor" << true;
        QTest::newRow("5.0 TiltSensor") << "5.0" << "TiltSensor" << true;
    }

    void alwaysOn()
    {
        QFETCH(QString, version);
        QFETCH(QString, element);
        QFETCH(bool, validSyntax);

        QQmlEngine engine;
        QString qml = QString("import QtQuick 2.0\nimport QtSensors %1\n%2 {\nalwaysOn: true\n}").arg(version).arg(element);
        QQmlComponent c(&engine);
        if (!validSyntax)
            QTest::ignoreMessage(QtWarningMsg, "QQmlComponent: Component is not ready");
        c.setData(qml.toLocal8Bit(), QUrl::fromLocalFile(QDir::currentPath()));
        QObject *obj = c.create();
        if (validSyntax) {
            QVERIFY(obj);
            QVariant alwaysOn = obj->property("alwaysOn");
            QCOMPARE(alwaysOn.isValid(), true);
            QCOMPARE(alwaysOn.toBool(), true);
            delete obj;
        } else {
            QCOMPARE(obj, static_cast<QObject*>(0));
        }
    }

    void namespace_api_data()
    {
        QTest::addColumn<QString>("qmlcode");
        QTest::addColumn<QVariant>("expected");

        QVariant expected;
        QStringList sl;
        foreach (const QByteArray &type, QSensor::sensorTypes()) {
            sl << QString::fromLocal8Bit(type);
            qDebug() << type;
        }
        expected = sl;
        QTest::newRow("Sensors.sensorTypes()")
                <<  "Item {\n"
                        "property var result\n"
                        "Component.onCompleted: {\n"
                            "result = Sensors.sensorTypes();\n"
                        "}\n"
                    "}"
                 << expected;

        foreach (const QByteArray &type, QSensor::sensorTypes()) {
            sl.clear();
            foreach (const QByteArray &identifier, QSensor::sensorsForType(type)) {
                sl << QString::fromLocal8Bit(identifier);
            }
            expected = sl;
            QTest::newRow(QString("Sensors.sensorsForType(\"%1\")").arg(QString::fromLocal8Bit(type)).toLocal8Bit().constData())
                    <<  QString(
                            "Item {\n"
                                "property var result\n"
                                "Component.onCompleted: {\n"
                                    "result = Sensors.sensorsForType(\"%1\");\n"
                                "}\n"
                            "}").arg(QString::fromLocal8Bit(type))
                     << expected;

            expected = QString::fromLocal8Bit(QSensor::defaultSensorForType(type));
            QTest::newRow(QString("Sensors.defaultSensorForType(\"%1\")").arg(QString::fromLocal8Bit(type)).toLocal8Bit().constData())
                    <<  QString(
                            "Item {\n"
                                "property var result\n"
                                "Component.onCompleted: {\n"
                                    "result = Sensors.defaultSensorForType(\"%1\");\n"
                                "}\n"
                            "}").arg(QString::fromLocal8Bit(type))
                     << expected;
        }
    }

    void namespace_api()
    {
        QFETCH(QString, qmlcode);
        QFETCH(QVariant, expected);

        QQmlEngine engine;
        QString qml = QString("import QtQuick 2.0\nimport QtSensors 5.0 as Sensors\n%1").arg(qmlcode);
        QQmlComponent c(&engine);
        c.setData(qml.toLocal8Bit(), QUrl::fromLocalFile(QDir::currentPath()));
        QObject *obj = c.create();
        QVERIFY(obj);
        QVariant result = obj->property("result");
        QCOMPARE(expected, result);
        delete obj;
    }

};

QTEST_MAIN(tst_legacy_sensors)

#include "tst_legacy_sensors.moc"
