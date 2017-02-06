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

#include <QtPositioning/qgeosatelliteinfo.h>

#include <QMetaType>
#include <QObject>
#include <QDebug>
#include <QTest>

#include <float.h>
#include <limits.h>

QT_USE_NAMESPACE
Q_DECLARE_METATYPE(QGeoSatelliteInfo)
Q_DECLARE_METATYPE(QGeoSatelliteInfo::Attribute)

QByteArray tst_qgeosatelliteinfo_debug;

void tst_qgeosatelliteinfo_messageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    switch (type) {
        case QtDebugMsg :
            tst_qgeosatelliteinfo_debug = msg.toLocal8Bit();
            break;
        default:
            break;
    }
}


QList<qreal> tst_qgeosatelliteinfo_qrealTestValues()
{
    QList<qreal> values;

    if (qreal(DBL_MIN) == DBL_MIN)
        values << DBL_MIN;

    values << FLT_MIN;
    values << -1.0 << 0.0 << 1.0;
    values << FLT_MAX;

    if (qreal(DBL_MAX) == DBL_MAX)
        values << DBL_MAX;

    return values;
}

QList<int> tst_qgeosatelliteinfo_intTestValues()
{
    QList<int> values;
    values << INT_MIN << -100 << 0 << 100 << INT_MAX;
    return values;
}

QList<QGeoSatelliteInfo::Attribute> tst_qgeosatelliteinfo_getAttributes()
{
    QList<QGeoSatelliteInfo::Attribute> attributes;
    attributes << QGeoSatelliteInfo::Elevation
            << QGeoSatelliteInfo::Azimuth;
    return attributes;
}


class tst_QGeoSatelliteInfo : public QObject
{
    Q_OBJECT

private:
    QGeoSatelliteInfo updateWithAttribute(QGeoSatelliteInfo::Attribute attribute, qreal value)
    {
        QGeoSatelliteInfo info;
        info.setAttribute(attribute, value);
        return info;
    }

    void addTestData_update()
    {
        QTest::addColumn<QGeoSatelliteInfo>("info");

        QList<int> intValues = tst_qgeosatelliteinfo_intTestValues();

        for (int i=0; i<intValues.count(); i++) {
            QGeoSatelliteInfo info;
            info.setSignalStrength(intValues[i]);
            QTest::newRow("signal strength") << info;
        }

        for (int i=0; i<intValues.count(); i++) {
            QGeoSatelliteInfo info;
            info.setSatelliteIdentifier(intValues[i]);
            QTest::newRow("satellite identifier") << info;
        }

            QGeoSatelliteInfo info;
            info.setSatelliteSystem(QGeoSatelliteInfo::GPS);
            QTest::newRow("satellite system") << info;
            info.setSatelliteSystem(QGeoSatelliteInfo::GLONASS);
            QTest::newRow("satellite system") << info;

        QList<QGeoSatelliteInfo::Attribute> attributes = tst_qgeosatelliteinfo_getAttributes();
        QList<qreal> qrealValues = tst_qgeosatelliteinfo_qrealTestValues();
        for (int i=0; i<attributes.count(); i++) {
            QTest::newRow(qPrintable(QString("Attribute %1 = %2").arg(attributes[i]).arg(qrealValues[i])))
                    << updateWithAttribute(attributes[i], qrealValues[i]);
        }
    }

private slots:
    void constructor()
    {
        QGeoSatelliteInfo info;
        QCOMPARE(info.signalStrength(), -1);
        QCOMPARE(info.satelliteIdentifier(), -1);
        QCOMPARE(info.satelliteSystem(), QGeoSatelliteInfo::Undefined);
        QList<QGeoSatelliteInfo::Attribute> attributes = tst_qgeosatelliteinfo_getAttributes();
        for (int i=0; i<attributes.count(); i++)
            QCOMPARE(info.attribute(attributes[i]), qreal(-1.0));
    }
    void constructor_copy()
    {
        QFETCH(QGeoSatelliteInfo, info);

        QCOMPARE(QGeoSatelliteInfo(info), info);
    }

    void constructor_copy_data()
    {
        addTestData_update();
    }

    void operator_comparison()
    {
        QFETCH(QGeoSatelliteInfo, info);

        QVERIFY(info == info);
        QCOMPARE(info != info, false);
        QCOMPARE(info == QGeoSatelliteInfo(), false);
        QCOMPARE(info != QGeoSatelliteInfo(), true);

        QVERIFY(QGeoSatelliteInfo() == QGeoSatelliteInfo());
    }

    void operator_comparison_data()
    {
        addTestData_update();
    }

    void operator_assign()
    {
        QFETCH(QGeoSatelliteInfo, info);

        QGeoSatelliteInfo info2 = info;
        QCOMPARE(info2, info);
    }

    void operator_assign_data()
    {
        addTestData_update();
    }

    void setSignalStrength()
    {
        QFETCH(int, signal);

        QGeoSatelliteInfo info;
        QCOMPARE(info.signalStrength(), -1);

        info.setSignalStrength(signal);
        QCOMPARE(info.signalStrength(), signal);
    }

    void setSignalStrength_data()
    {
        QTest::addColumn<int>("signal");

        QList<int> intValues = tst_qgeosatelliteinfo_intTestValues();
        for (int i=0; i<intValues.count(); i++)
            QTest::newRow(qPrintable(QString("%1").arg(intValues[i]))) << intValues[i];
    }
    void setSatelliteIdentifier()
    {
        QFETCH(int, satId);

        QGeoSatelliteInfo info;
        QCOMPARE(info.satelliteIdentifier(), -1);

        info.setSatelliteIdentifier(satId);
        QCOMPARE(info.satelliteIdentifier(), satId);
    }

    void setSatelliteIdentifier_data()
    {
        QTest::addColumn<int>("satId");

        QList<int> intValues = tst_qgeosatelliteinfo_intTestValues();
        for (int i=0; i<intValues.count(); i++)
            QTest::newRow(qPrintable(QString("%1").arg(intValues[i]))) << intValues[i];
    }

    void setSatelliteSystem()
    {
        QFETCH(int, system);

        QGeoSatelliteInfo info;
        QCOMPARE(info.satelliteSystem(), QGeoSatelliteInfo::Undefined);

        info.setSatelliteSystem(static_cast<QGeoSatelliteInfo::SatelliteSystem>(system));
        QCOMPARE(info.satelliteSystem(), static_cast<QGeoSatelliteInfo::SatelliteSystem>(system));
    }

    void setSatelliteSystem_data()
    {
        QTest::addColumn<int>("system");

        QTest::newRow("Sat system undefined")
        << int(QGeoSatelliteInfo::Undefined);
        QTest::newRow("Sat system GPS")
        << int(QGeoSatelliteInfo::GPS);
        QTest::newRow("Sat system GLONASS")
        << int(QGeoSatelliteInfo::GLONASS);
    }

    void attribute()
    {
        QFETCH(QGeoSatelliteInfo::Attribute, attribute);
        QFETCH(qreal, value);

        QGeoSatelliteInfo u;
        QCOMPARE(u.attribute(attribute), qreal(-1.0));

        u.setAttribute(attribute, value);
        QCOMPARE(u.attribute(attribute), value);
        u.removeAttribute(attribute);
        QCOMPARE(u.attribute(attribute), qreal(-1.0));
    }

    void attribute_data()
    {
        QTest::addColumn<QGeoSatelliteInfo::Attribute>("attribute");
        QTest::addColumn<qreal>("value");

        QList<QGeoSatelliteInfo::Attribute> props;
        props << QGeoSatelliteInfo::Elevation
              << QGeoSatelliteInfo::Azimuth;
        for (int i=0; i<props.count(); i++) {
            QTest::newRow(qPrintable(QString("Attribute %1 = -1.0").arg(props[i])))
                << props[i]
                << qreal(-1.0);
            QTest::newRow(qPrintable(QString("Attribute %1 = 0.0").arg(props[i])))
                << props[i]
                << qreal(0.0);
            QTest::newRow(qPrintable(QString("Attribute %1 = 1.0").arg(props[i])))
                << props[i]
                << qreal(1.0);
        }
    }

    void hasAttribute()
    {
        QFETCH(QGeoSatelliteInfo::Attribute, attribute);
        QFETCH(qreal, value);

        QGeoSatelliteInfo u;
        QVERIFY(!u.hasAttribute(attribute));

        u.setAttribute(attribute, value);
        QVERIFY(u.hasAttribute(attribute));

        u.removeAttribute(attribute);
        QVERIFY(!u.hasAttribute(attribute));
    }

    void hasAttribute_data()
    {
        attribute_data();
    }

    void removeAttribute()
    {
        QFETCH(QGeoSatelliteInfo::Attribute, attribute);
        QFETCH(qreal, value);

        QGeoSatelliteInfo u;
        QVERIFY(!u.hasAttribute(attribute));

        u.setAttribute(attribute, value);
        QVERIFY(u.hasAttribute(attribute));

        u.removeAttribute(attribute);
        QVERIFY(!u.hasAttribute(attribute));

        u.setAttribute(attribute, value);
        QVERIFY(u.hasAttribute(attribute));
    }

    void removeAttribute_data()
    {
        attribute_data();
    }

    void datastream()
    {
        QFETCH(QGeoSatelliteInfo, info);

        QByteArray ba;
        QDataStream out(&ba, QIODevice::WriteOnly);
        out << info;

        QDataStream in(&ba, QIODevice::ReadOnly);
        QGeoSatelliteInfo inInfo;
        in >> inInfo;
        QCOMPARE(inInfo, info);
    }

    void datastream_data()
    {
        addTestData_update();
    }

    void debug()
    {
        QFETCH(QGeoSatelliteInfo, info);
        QFETCH(int, nextValue);
        QFETCH(QByteArray, debugString);

        qInstallMessageHandler(tst_qgeosatelliteinfo_messageHandler);
        qDebug() << info << nextValue;
        qInstallMessageHandler(0);
        QCOMPARE(QString(tst_qgeosatelliteinfo_debug), QString(debugString));
    }

    void debug_data()
    {
        QTest::addColumn<QGeoSatelliteInfo>("info");
        QTest::addColumn<int>("nextValue");
        QTest::addColumn<QByteArray>("debugString");

        QGeoSatelliteInfo info;

        QTest::newRow("uninitialized") << info << 45
                << QByteArray("QGeoSatelliteInfo(system=0, satId=-1, signal-strength=-1) 45");

        info = QGeoSatelliteInfo();
        info.setSignalStrength(1);
        QTest::newRow("with SignalStrength") << info << 60
                << QByteArray("QGeoSatelliteInfo(system=0, satId=-1, signal-strength=1) 60");

        info = QGeoSatelliteInfo();
        info.setSatelliteIdentifier(1);
        QTest::newRow("with SatelliteIdentifier") << info << -1
                << QByteArray("QGeoSatelliteInfo(system=0, satId=1, signal-strength=-1) -1");

        info = QGeoSatelliteInfo();
        info.setSatelliteSystem(QGeoSatelliteInfo::GPS);
        QTest::newRow("with System GPS") << info << 1
                << QByteArray("QGeoSatelliteInfo(system=1, satId=-1, signal-strength=-1) 1");

        info = QGeoSatelliteInfo();
        info.setSatelliteSystem(QGeoSatelliteInfo::GLONASS);
        QTest::newRow("with System GLONASS") << info << 56
                << QByteArray("QGeoSatelliteInfo(system=2, satId=-1, signal-strength=-1) 56");

        info = QGeoSatelliteInfo();
        info.setAttribute(QGeoSatelliteInfo::Elevation, 1.1);
        QTest::newRow("with Elevation") << info << 0
                << QByteArray("QGeoSatelliteInfo(system=0, satId=-1, signal-strength=-1, Elevation=1.1) 0");

        info = QGeoSatelliteInfo();
        info.setAttribute(QGeoSatelliteInfo::Azimuth, 1.1);
        QTest::newRow("with Azimuth") << info << 45
                << QByteArray("QGeoSatelliteInfo(system=0, satId=-1, signal-strength=-1, Azimuth=1.1) 45");
    }
};


QTEST_APPLESS_MAIN(tst_QGeoSatelliteInfo)
#include "tst_qgeosatelliteinfo.moc"
