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

#include "../utils/qlocationtestutils_p.h"

#include <QtPositioning/qgeocoordinate.h>
#include <qtest.h>

#include <QMetaType>
#include <QDebug>

#include <float.h>

QT_USE_NAMESPACE

Q_DECLARE_METATYPE(QGeoCoordinate::CoordinateFormat)
Q_DECLARE_METATYPE(QGeoCoordinate::CoordinateType)

static const QGeoCoordinate BRISBANE(-27.46758, 153.027892);
static const QGeoCoordinate MELBOURNE(-37.814251, 144.963169);
static const QGeoCoordinate LONDON(51.500152, -0.126236);
static const QGeoCoordinate NEW_YORK(40.71453, -74.00713);
static const QGeoCoordinate NORTH_POLE(90, 0);
static const QGeoCoordinate SOUTH_POLE(-90, 0);

static const QChar DEGREES_SYMB(0x00B0);


QByteArray tst_qgeocoordinate_debug;

void tst_qgeocoordinate_messageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    switch (type) {
        case QtDebugMsg :
            tst_qgeocoordinate_debug = msg.toLocal8Bit();
            break;
        default:
            break;
    }
}


class tst_QGeoCoordinate : public QObject
{
    Q_OBJECT

private:
    enum TestDataType {
        Latitude,
        Longitude,
        Altitude
    };

private slots:
    void initTestcase()
    {
        qRegisterMetaType<QGeoCoordinate>();
        QMetaType::registerEqualsComparator<QGeoCoordinate>();
    }

    void constructor()
    {
        QGeoCoordinate c;
        QVERIFY(!c.isValid());
        QCOMPARE(c, QGeoCoordinate());
    }

    void constructor_lat_long()
    {
        QFETCH(double, latitude);
        QFETCH(double, longitude);
        QFETCH(QGeoCoordinate::CoordinateType, type);

        QGeoCoordinate c(latitude, longitude);
        QCOMPARE(c.type(), type);
        if (type != QGeoCoordinate::InvalidCoordinate) {
            QVERIFY(c.isValid());
            QCOMPARE(c.latitude(), latitude);
            QCOMPARE(c.longitude(), longitude);
        } else {
            QVERIFY(!c.isValid());
            QVERIFY(c.latitude() != latitude);
            QVERIFY(c.longitude() != longitude);
        }
    }

    void constructor_lat_long_data()
    {
        QTest::addColumn<double>("latitude");
        QTest::addColumn<double>("longitude");
        QTest::addColumn<QGeoCoordinate::CoordinateType>("type");

        QTest::newRow("both zero") << 0.0 << 0.0 << QGeoCoordinate::Coordinate2D;
        QTest::newRow("both negative") << -1.0 << -1.0 << QGeoCoordinate::Coordinate2D;
        QTest::newRow("both positive") << 1.0 << 1.0 << QGeoCoordinate::Coordinate2D;
        QTest::newRow("latitude negative") << -1.0 << 1.0 << QGeoCoordinate::Coordinate2D;
        QTest::newRow("longitude negative") << 1.0 << -1.0 << QGeoCoordinate::Coordinate2D;

        QTest::newRow("both too high") << 90.1 << 180.1 << QGeoCoordinate::InvalidCoordinate;
        QTest::newRow("latitude too high") << 90.1 << 0.1 << QGeoCoordinate::InvalidCoordinate;
        QTest::newRow("longitude too high") << 0.1 << 180.1 << QGeoCoordinate::InvalidCoordinate;

        QTest::newRow("both too low") << -90.1 << -180.1 << QGeoCoordinate::InvalidCoordinate;
        QTest::newRow("latitude too low") << -90.1 << 0.1 << QGeoCoordinate::InvalidCoordinate;
        QTest::newRow("longitude too low") << 0.1 << -180.1 << QGeoCoordinate::InvalidCoordinate;

        QTest::newRow("both not too high") << 90.0 << 180.0 << QGeoCoordinate::Coordinate2D;
        QTest::newRow("both not too low") << -90.0 << -180.0 << QGeoCoordinate::Coordinate2D;

        QTest::newRow("latitude too high and longitude too low") << 90.1 << -180.1 << QGeoCoordinate::InvalidCoordinate;
        QTest::newRow("latitude too low and longitude too high") << -90.1 << 180.1 << QGeoCoordinate::InvalidCoordinate;
    }

    void constructor_lat_long_alt()
    {
        QFETCH(double, latitude);
        QFETCH(double, longitude);
        QFETCH(double, altitude);
        QFETCH(QGeoCoordinate::CoordinateType, type);

        QGeoCoordinate c(latitude, longitude, altitude);
        QCOMPARE(c.type(), type);
        if (type != QGeoCoordinate::InvalidCoordinate) {
            QCOMPARE(c.latitude(), latitude);
            QCOMPARE(c.longitude(), longitude);
            QCOMPARE(c.altitude(), altitude);
        } else {
            QVERIFY(!c.isValid());
            QVERIFY(c.latitude() != latitude);
            QVERIFY(c.longitude() != longitude);
            QVERIFY(c.altitude() != altitude);
        }
    }

    void constructor_lat_long_alt_data()
    {
        QTest::addColumn<double>("latitude");
        QTest::addColumn<double>("longitude");
        QTest::addColumn<double>("altitude");
        QTest::addColumn<QGeoCoordinate::CoordinateType>("type");

        QTest::newRow("all zero") << 0.0 << 0.0 << 0.0 << QGeoCoordinate::Coordinate3D;
        QTest::newRow("all negative") << -1.0 << -1.0 << -2.0 << QGeoCoordinate::Coordinate3D;
        QTest::newRow("all positive") << 1.0 << 1.0 << 4.0 << QGeoCoordinate::Coordinate3D;

        QTest::newRow("latitude negative") << -1.0 << 1.0 << 1.0 << QGeoCoordinate::Coordinate3D;
        QTest::newRow("longitude negative") << 1.0 << -1.0  << 1.0 << QGeoCoordinate::Coordinate3D;
        QTest::newRow("altitude negative") << 1.0 << 1.0  << -1.0 << QGeoCoordinate::Coordinate3D;

        QTest::newRow("altitude not too high") << 1.0 << 1.0  << DBL_MAX << QGeoCoordinate::Coordinate3D;
        QTest::newRow("altitude not too low") << 1.0 << 1.0  << DBL_MIN << QGeoCoordinate::Coordinate3D;

        QTest::newRow("all not too high") << 90.0 << 180.0  << DBL_MAX << QGeoCoordinate::Coordinate3D;
        QTest::newRow("all not too low") << -90.0 << -180.0  << DBL_MIN << QGeoCoordinate::Coordinate3D;

        QTest::newRow("all too high") << 90.1 << 180.1 << DBL_MAX << QGeoCoordinate::InvalidCoordinate;
        QTest::newRow("all too low") << -90.1 << -180.1 << DBL_MIN << QGeoCoordinate::InvalidCoordinate;
    }

    void copy_constructor()
    {
        QFETCH(QGeoCoordinate, c);

        QGeoCoordinate copy(c);
        QCOMPARE(copy.type(), c.type());
        if (c.type() != QGeoCoordinate::InvalidCoordinate) {
            QCOMPARE(copy.latitude(), c.latitude());
            QCOMPARE(copy.longitude(), c.longitude());
            if (c.type() == QGeoCoordinate::Coordinate3D)
                QCOMPARE(copy.altitude(), c.altitude());
        }
    }

    void copy_constructor_data()
    {
        QTest::addColumn<QGeoCoordinate>("c");

        QTest::newRow("no argument") << QGeoCoordinate();
        QTest::newRow("latitude, longitude arguments all zero") << QGeoCoordinate(0.0, 0.0);

        QTest::newRow("latitude, longitude arguments not too high") << QGeoCoordinate(90.0, 180.0);
        QTest::newRow("latitude, longitude arguments not too low") << QGeoCoordinate(-90.0, -180.0);
        QTest::newRow("latitude, longitude arguments too high") << QGeoCoordinate(90.1, 180.1);
        QTest::newRow("latitude, longitude arguments too low") << QGeoCoordinate(-90.1, -180.1);

        QTest::newRow("latitude, longitude, altitude arguments all zero") << QGeoCoordinate(0.0, 0.0, 0.0);
        QTest::newRow("latitude, longitude, altitude arguments not too high values") << QGeoCoordinate(90.0, 180.0, DBL_MAX);
        QTest::newRow("latitude, longitude, altitude arguments not too low values") << QGeoCoordinate(-90.0, -180.0, DBL_MIN);

        QTest::newRow("latitude, longitude, altitude arguments too high latitude & longitude") << QGeoCoordinate(90.1, 180.1, DBL_MAX);
        QTest::newRow("latitude, longitude, altitude arguments too low latitude & longitude") << QGeoCoordinate(-90.1, -180.1, DBL_MAX);
    }

    void destructor()
    {
        QGeoCoordinate *coordinate;

        coordinate = new QGeoCoordinate();
        delete coordinate;

        coordinate = new QGeoCoordinate(0.0, 0.0);
        delete coordinate;

        coordinate = new QGeoCoordinate(0.0, 0.0, 0.0);
        delete coordinate;

        coordinate = new QGeoCoordinate(90.0, 180.0);
        delete coordinate;

        coordinate = new QGeoCoordinate(-90.0, -180.0);
        delete coordinate;

        coordinate = new QGeoCoordinate(90.1, 180.1);
        delete coordinate;

        coordinate = new QGeoCoordinate(-90.1, -180.1);
        delete coordinate;
    }

    void destructor2()
    {
        QFETCH(QGeoCoordinate, c);
        QGeoCoordinate *coordinate = new QGeoCoordinate(c);
        delete coordinate;
    }

    void destructor2_data()
    {
        copy_constructor_data();
    }

    void assign()
    {
        QFETCH(QGeoCoordinate, c);

        QGeoCoordinate c1 = c;
        QCOMPARE(c.type(), c1.type());
        if (c.isValid()) {
            QCOMPARE(c.latitude(), c.latitude());
            QCOMPARE(c.longitude(), c.longitude());
            if (c.type() == QGeoCoordinate::Coordinate3D)
                QCOMPARE(c.altitude(), c.altitude());
        }
    }

    void assign_data()
    {
        copy_constructor_data();
    }

    void comparison()
    {
        QFETCH(QGeoCoordinate, c1);
        QFETCH(QGeoCoordinate, c2);
        QFETCH(bool, result);

        QCOMPARE(c1 == c2, result);
        QVariant v1 = QVariant::fromValue(c1);
        QVariant v2 = QVariant::fromValue(c2);
        QCOMPARE(v2 == v1, result);
    }

    void comparison_data()
    {
        QTest::addColumn<QGeoCoordinate>("c1");
        QTest::addColumn<QGeoCoordinate>("c2");
        QTest::addColumn<bool>("result");

        QTest::newRow("Invalid != BRISBANE")
                << QGeoCoordinate(-190,-1000) << BRISBANE << false;
        QTest::newRow("BRISBANE != MELBOURNE")
                << BRISBANE << MELBOURNE << false;
        QTest::newRow("equal")
                << BRISBANE << BRISBANE << true;
        QTest::newRow("LONDON != uninitialized data")
                << LONDON << QGeoCoordinate() << false;
        QTest::newRow("uninitialized data == uninitialized data")
                << QGeoCoordinate() << QGeoCoordinate() << true;
        QTest::newRow("invalid == same invalid")
                << QGeoCoordinate(-190,-1000) << QGeoCoordinate(-190,-1000) << true;
        QTest::newRow("invalid == different invalid")
                << QGeoCoordinate(-190,-1000) << QGeoCoordinate(190,1000) << true;
        QTest::newRow("valid != another valid")
                << QGeoCoordinate(-90,-180) << QGeoCoordinate(-45,+45) << false;
        QTest::newRow("valid == same valid")
                << QGeoCoordinate(-90,-180) << QGeoCoordinate(-90,-180) << true;
        QTest::newRow("different longitudes at north pole are equal")
                << QGeoCoordinate(90, 0) << QGeoCoordinate(90, 45) << true;
        QTest::newRow("different longitudes at south pole are equal")
                << QGeoCoordinate(-90, 0) << QGeoCoordinate(-90, 45) << true;
    }

    void type()
    {
        QFETCH(QGeoCoordinate, c);
        QFETCH(QGeoCoordinate::CoordinateType, type);

        QCOMPARE(c.type(), type);
    }

    void type_data()
    {
        QTest::addColumn<QGeoCoordinate>("c");
        QTest::addColumn<QGeoCoordinate::CoordinateType>("type");

        QGeoCoordinate c;

        QTest::newRow("no values set")  << c << QGeoCoordinate::InvalidCoordinate;

        c.setAltitude(1.0);
        QTest::newRow("only altitude is set")  << c << QGeoCoordinate::InvalidCoordinate;

        c.setLongitude(1.0);
        QTest::newRow("only latitude and altitude is set") << c << QGeoCoordinate::InvalidCoordinate;

        c.setLatitude(-1.0);
        QTest::newRow("all valid: 3D Coordinate") << c << QGeoCoordinate::Coordinate3D;

        c.setLatitude(-90.1);
        QTest::newRow("too low latitude and valid longitude") << c << QGeoCoordinate::InvalidCoordinate;

        c.setLongitude(-180.1);
        c.setLatitude(90.0);
        QTest::newRow("valid latitude and too low longitude") << c << QGeoCoordinate::InvalidCoordinate;

        c.setLatitude(90.1);
        c.setLongitude(-180.0);
        QTest::newRow("too high latitude and valid longitude") << c << QGeoCoordinate::InvalidCoordinate;

        c.setLatitude(-90.0);
        c.setLongitude(180.1);
        QTest::newRow("valid latitude and too high longitude") << c << QGeoCoordinate::InvalidCoordinate;
    }

    void valid()
    {
        QFETCH(QGeoCoordinate, c);
        QCOMPARE(c.isValid(), c.type() != QGeoCoordinate::InvalidCoordinate);
    }

    void valid_data()
    {
        type_data();
    }

    void addDataValues(TestDataType type)
    {
        QTest::addColumn<double>("value");
        QTest::addColumn<bool>("valid");

        QTest::newRow("negative") << -1.0 << true;
        QTest::newRow("zero") << 0.0 << true;
        QTest::newRow("positive") << 1.0 << true;

        switch (type) {
            case Latitude:
                QTest::newRow("too low") << -90.1 << false;
                QTest::newRow("not too low") << -90.0 << true;
                QTest::newRow("not too hight") << 90.0 << true;
                QTest::newRow("too high") << 90.1;
                break;
            case Longitude:
                QTest::newRow("too low") << -180.1 << false;
                QTest::newRow("not too low") << -180.0 << true;
                QTest::newRow("not too hight") << 180.0 << true;
                QTest::newRow("too high") << 180.1;
                break;
            case Altitude:
                break;
        }
    }

    void latitude()
    {
        QFETCH(double, value);
        QGeoCoordinate c;
        c.setLatitude(value);
        QCOMPARE(c.latitude(), value);

        QGeoCoordinate c2 = c;
        QCOMPARE(c.latitude(), value);
        QCOMPARE(c2, c);
    }
    void latitude_data() { addDataValues(Latitude); }

    void longitude()
    {
        QFETCH(double, value);
        QGeoCoordinate c;
        c.setLongitude(value);
        QCOMPARE(c.longitude(), value);

        QGeoCoordinate c2 = c;
        QCOMPARE(c.longitude(), value);
        QCOMPARE(c2, c);
    }
    void longitude_data() { addDataValues(Longitude); }

    void altitude()
    {
        QFETCH(double, value);
        QGeoCoordinate c;
        c.setAltitude(value);
        QCOMPARE(c.altitude(), value);

        QGeoCoordinate c2 = c;
        QCOMPARE(c.altitude(), value);
        QCOMPARE(c2, c);
    }
    void altitude_data() { addDataValues(Altitude); }

    void distanceTo()
    {
        QFETCH(QGeoCoordinate, c1);
        QFETCH(QGeoCoordinate, c2);
        QFETCH(qreal, distance);

        QCOMPARE(QString::number(c1.distanceTo(c2)), QString::number(distance));
    }

    void distanceTo_data()
    {
        QTest::addColumn<QGeoCoordinate>("c1");
        QTest::addColumn<QGeoCoordinate>("c2");
        QTest::addColumn<qreal>("distance");

        QTest::newRow("invalid coord 1")
                << QGeoCoordinate() << BRISBANE << qreal(0.0);
        QTest::newRow("invalid coord 2")
                << BRISBANE << QGeoCoordinate() << qreal(0.0);
        QTest::newRow("brisbane -> melbourne")
                << BRISBANE << MELBOURNE << qreal(1374820.1618767744);
        QTest::newRow("london -> new york")
                << LONDON << NEW_YORK << qreal(5570538.4987236429);
        QTest::newRow("north pole -> south pole")
                << NORTH_POLE << SOUTH_POLE << qreal(20015109.4154876769);
    }

    void azimuthTo()
    {
        QFETCH(QGeoCoordinate, c1);
        QFETCH(QGeoCoordinate, c2);
        QFETCH(qreal, azimuth);

        qreal result = c1.azimuthTo(c2);
        QVERIFY(result >= 0.0);
        QVERIFY(result < 360.0);
        QCOMPARE(QString::number(result), QString::number(azimuth));
    }

    void azimuthTo_data()
    {
        QTest::addColumn<QGeoCoordinate>("c1");
        QTest::addColumn<QGeoCoordinate>("c2");
        QTest::addColumn<qreal>("azimuth");

        QTest::newRow("invalid coord 1")
                << QGeoCoordinate() << BRISBANE << qreal(0.0);
        QTest::newRow("invalid coord 2")
                << BRISBANE << QGeoCoordinate() << qreal(0.0);
        QTest::newRow("brisbane -> melbourne")
                << BRISBANE << MELBOURNE << qreal(211.1717286649);
        QTest::newRow("london -> new york")
                << LONDON << NEW_YORK << qreal(288.3388804508);
        QTest::newRow("north pole -> south pole")
                << NORTH_POLE << SOUTH_POLE << qreal(180.0);
        QTest::newRow("Almost 360degrees bearing")
                << QGeoCoordinate(0.5,45.0,0.0) << QGeoCoordinate(0.5,-134.9999651,0.0)  << qreal(359.998);
    }

    void atDistanceAndAzimuth()
    {
        QFETCH(QGeoCoordinate, origin);
        QFETCH(qreal, distance);
        QFETCH(qreal, azimuth);
        QFETCH(QGeoCoordinate, result);

        QCOMPARE(result, origin.atDistanceAndAzimuth(distance, azimuth));
    }

    void atDistanceAndAzimuth_data()
    {
        QTest::addColumn<QGeoCoordinate>("origin");
        QTest::addColumn<qreal>("distance");
        QTest::addColumn<qreal>("azimuth");
        QTest::addColumn<QGeoCoordinate>("result");

        QTest::newRow("invalid coord")
            << QGeoCoordinate()
            << qreal(1000.0)
            << qreal(10.0)
            << QGeoCoordinate();
        if (sizeof(qreal) == sizeof(double)) {
            QTest::newRow("brisbane -> melbourne")
                << BRISBANE
                << qreal(1374820.1618767744)
                << qreal(211.1717286649)
                << MELBOURNE;
            QTest::newRow("london -> new york")
                << LONDON
                << qreal(5570538.4987236429)
                << qreal(288.3388804508)
                << NEW_YORK;
            QTest::newRow("north pole -> south pole")
                << NORTH_POLE
                << qreal(20015109.4154876769)
                << qreal(180.0)
                << SOUTH_POLE;
        } else {
            QTest::newRow("brisbane -> melbourne")
                << BRISBANE
                << qreal(1374820.1618767744)
                << qreal(211.1717286649)
                << QGeoCoordinate(-37.8142515084775, 144.963170622944);
            QTest::newRow("london -> new york")
                << LONDON
                << qreal(5570538.4987236429)
                << qreal(288.3388804508)
                << QGeoCoordinate(40.7145220608416, -74.0071216045375);
            QTest::newRow("north pole -> south pole")
                << NORTH_POLE
                << qreal(20015109.4154876769)
                << qreal(180.0)
                << QGeoCoordinate(-89.9999947369857, -90.0);
        }
    }

    void degreesToString()
    {
        QFETCH(QGeoCoordinate, coord);
        QFETCH(QGeoCoordinate::CoordinateFormat, format);
        QFETCH(QString, string);

        QCOMPARE(coord.toString(format), string);
    }

    void degreesToString_data()
    {
        QTest::addColumn<QGeoCoordinate>("coord");
        QTest::addColumn<QGeoCoordinate::CoordinateFormat>("format");
        QTest::addColumn<QString>("string");

        QGeoCoordinate northEast(27.46758, 153.027892);
        QGeoCoordinate northEastWithAlt(27.46758, 153.027892, 28.23411);
        QGeoCoordinate southEast(-27.46758, 153.027892);
        QGeoCoordinate southEastWithAlt(-27.46758, 153.027892, 28.23411);
        QGeoCoordinate northWest(27.46758, -153.027892);
        QGeoCoordinate northWestWithAlt(27.46758, -153.027892, 28.23411);
        QGeoCoordinate southWest(-27.46758, -153.027892);
        QGeoCoordinate southWestWithAlt(-27.46758, -153.027892, 28.23411);

        QGeoCoordinate empty;
        QGeoCoordinate toohigh(90.1, 180.1);
        QGeoCoordinate toolow(-90.1, -180.1);
        QGeoCoordinate zeroLatLong(0.0, 0.0);
        QGeoCoordinate allZero(0.0, 0.0, 0.0);

        QTest::newRow("empty, dd, no hemisphere")
                << empty << QGeoCoordinate::Degrees
                << QString();
        QTest::newRow("empty, dd, hemisphere")
                << empty << QGeoCoordinate::DegreesWithHemisphere
                << QString();
        QTest::newRow("empty, dm, no hemisphere")
                << empty << QGeoCoordinate::DegreesMinutes
                << QString();
        QTest::newRow("empty, dm, hemisphere")
                << empty << QGeoCoordinate::DegreesMinutesWithHemisphere
                << QString();
        QTest::newRow("empty, dms, no hemisphere")
                << empty << QGeoCoordinate::DegreesMinutesSeconds
                << QString();
        QTest::newRow("empty, dms, hemisphere")
                << empty << QGeoCoordinate::DegreesMinutesSecondsWithHemisphere
                << QString();

        QTest::newRow("too low, dd, no hemisphere")
                << toolow << QGeoCoordinate::Degrees
                << QString();
        QTest::newRow("too low, dd, hemisphere")
                << toolow << QGeoCoordinate::DegreesWithHemisphere
                << QString();
        QTest::newRow("too low, dm, no hemisphere")
                << toolow << QGeoCoordinate::DegreesMinutes
                << QString();
        QTest::newRow("too low, dm, hemisphere")
                << toolow << QGeoCoordinate::DegreesMinutesWithHemisphere
                << QString();
        QTest::newRow("too low, dms, no hemisphere")
                << toolow << QGeoCoordinate::DegreesMinutesSeconds
                << QString();
        QTest::newRow("too low, dms, hemisphere")
                << toolow << QGeoCoordinate::DegreesMinutesSecondsWithHemisphere
                << QString();

        QTest::newRow("too high, dd, no hemisphere")
                << toohigh << QGeoCoordinate::Degrees
                << QString();
        QTest::newRow("too high, dd, hemisphere")
                << toohigh << QGeoCoordinate::DegreesWithHemisphere
                << QString();
        QTest::newRow("too high, dm, no hemisphere")
                << toohigh << QGeoCoordinate::DegreesMinutes
                << QString();
        QTest::newRow("too high, dm, hemisphere")
                << toohigh << QGeoCoordinate::DegreesMinutesWithHemisphere
                << QString();
        QTest::newRow("too high, dms, no hemisphere")
                << toohigh << QGeoCoordinate::DegreesMinutesSeconds
                << QString();
        QTest::newRow("too high, dms, hemisphere")
                << toohigh << QGeoCoordinate::DegreesMinutesSecondsWithHemisphere
                << QString();

        QTest::newRow("zeroLatLong, dd, no hemisphere")
                << zeroLatLong << QGeoCoordinate::Degrees
                << QString("0.00000%1, 0.00000%1").arg(DEGREES_SYMB);
        QTest::newRow("zeroLatLong, dd, hemisphere")
                << zeroLatLong << QGeoCoordinate::DegreesWithHemisphere
                << QString("0.00000%1, 0.00000%1").arg(DEGREES_SYMB);
        QTest::newRow("zeroLatLong, dm, no hemisphere")
                << zeroLatLong << QGeoCoordinate::DegreesMinutes
                << QString("0%1 0.000', 0%1 0.000'").arg(DEGREES_SYMB);
        QTest::newRow("zeroLatLong, dm, hemisphere")
                << zeroLatLong << QGeoCoordinate::DegreesMinutesWithHemisphere
                << QString("0%1 0.000', 0%1 0.000'").arg(DEGREES_SYMB);
        QTest::newRow("zeroLatLong, dms, no hemisphere")
                << zeroLatLong << QGeoCoordinate::DegreesMinutesSeconds
                << QString("0%1 0' 0.0\", 0%1 0' 0.0\"").arg(DEGREES_SYMB);
        QTest::newRow("zeroLatLong, dms, hemisphere")
                << zeroLatLong << QGeoCoordinate::DegreesMinutesSecondsWithHemisphere
                << QString("0%1 0' 0.0\", 0%1 0' 0.0\"").arg(DEGREES_SYMB);

        QTest::newRow("allZero, dd, no hemisphere")
                << allZero << QGeoCoordinate::Degrees
                << QString("0.00000%1, 0.00000%1, 0m").arg(DEGREES_SYMB);
        QTest::newRow("allZero, dd, hemisphere")
                << allZero << QGeoCoordinate::DegreesWithHemisphere
                << QString("0.00000%1, 0.00000%1, 0m").arg(DEGREES_SYMB);
        QTest::newRow("allZero, dm, no hemisphere")
                << allZero << QGeoCoordinate::DegreesMinutes
                << QString("0%1 0.000', 0%1 0.000', 0m").arg(DEGREES_SYMB);
        QTest::newRow("allZero, dm, hemisphere")
                << allZero << QGeoCoordinate::DegreesMinutesWithHemisphere
                << QString("0%1 0.000', 0%1 0.000', 0m").arg(DEGREES_SYMB);
        QTest::newRow("allZero, dms, no hemisphere")
                << allZero << QGeoCoordinate::DegreesMinutesSeconds
                << QString("0%1 0' 0.0\", 0%1 0' 0.0\", 0m").arg(DEGREES_SYMB);
        QTest::newRow("allZero, dms, hemisphere")
                << allZero << QGeoCoordinate::DegreesMinutesSecondsWithHemisphere
                << QString("0%1 0' 0.0\", 0%1 0' 0.0\", 0m").arg(DEGREES_SYMB);

        QTest::newRow("NE, dd, no hemisphere")
                << northEast << QGeoCoordinate::Degrees
                << QString("27.46758%1, 153.02789%1").arg(DEGREES_SYMB);
        QTest::newRow("NE, dd, hemisphere")
                << northEast << QGeoCoordinate::DegreesWithHemisphere
                << QString("27.46758%1 N, 153.02789%1 E").arg(DEGREES_SYMB);
        QTest::newRow("NE, dm, no hemisphere")
                << northEast << QGeoCoordinate::DegreesMinutes
                << QString("27%1 28.055', 153%1 1.674'").arg(DEGREES_SYMB);
        QTest::newRow("NE, dm, hemisphere")
                << northEast << QGeoCoordinate::DegreesMinutesWithHemisphere
                << QString("27%1 28.055' N, 153%1 1.674' E").arg(DEGREES_SYMB);
        QTest::newRow("NE, dms, no hemisphere")
                << northEast << QGeoCoordinate::DegreesMinutesSeconds
                << QString("27%1 28' 3.3\", 153%1 1' 40.4\"").arg(DEGREES_SYMB);
        QTest::newRow("NE, dms, hemisphere")
                << northEast << QGeoCoordinate::DegreesMinutesSecondsWithHemisphere
                << QString("27%1 28' 3.3\" N, 153%1 1' 40.4\" E").arg(DEGREES_SYMB);

        QTest::newRow("NE with alt, dd, no hemisphere")
                << northEastWithAlt << QGeoCoordinate::Degrees
                << QString("27.46758%1, 153.02789%1, 28.2341m").arg(DEGREES_SYMB);
        QTest::newRow("NE with alt, dd, hemisphere")
                << northEastWithAlt << QGeoCoordinate::DegreesWithHemisphere
                << QString("27.46758%1 N, 153.02789%1 E, 28.2341m").arg(DEGREES_SYMB);
        QTest::newRow("NE with alt, dm, no hemisphere")
                << northEastWithAlt << QGeoCoordinate::DegreesMinutes
                << QString("27%1 28.055', 153%1 1.674', 28.2341m").arg(DEGREES_SYMB);
        QTest::newRow("NE with alt, dm, hemisphere")
                << northEastWithAlt << QGeoCoordinate::DegreesMinutesWithHemisphere
                << QString("27%1 28.055' N, 153%1 1.674' E, 28.2341m").arg(DEGREES_SYMB);
        QTest::newRow("NE with alt, dms, no hemisphere")
                << northEastWithAlt << QGeoCoordinate::DegreesMinutesSeconds
                << QString("27%1 28' 3.3\", 153%1 1' 40.4\", 28.2341m").arg(DEGREES_SYMB);
        QTest::newRow("NE with alt, dms, hemisphere")
                << northEastWithAlt << QGeoCoordinate::DegreesMinutesSecondsWithHemisphere
                << QString("27%1 28' 3.3\" N, 153%1 1' 40.4\" E, 28.2341m").arg(DEGREES_SYMB);

        QTest::newRow("SE, dd, no hemisphere")
                << southEast << QGeoCoordinate::Degrees
                << QString("-27.46758%1, 153.02789%1").arg(DEGREES_SYMB);
        QTest::newRow("SE, dd, hemisphere")
                << southEast << QGeoCoordinate::DegreesWithHemisphere
                << QString("27.46758%1 S, 153.02789%1 E").arg(DEGREES_SYMB);
        QTest::newRow("SE, dm, no hemisphere")
                << southEast << QGeoCoordinate::DegreesMinutes
                << QString("-27%1 28.055', 153%1 1.674'").arg(DEGREES_SYMB);
        QTest::newRow("SE, dm, hemisphere")
                << southEast << QGeoCoordinate::DegreesMinutesWithHemisphere
                << QString("27%1 28.055' S, 153%1 1.674' E").arg(DEGREES_SYMB);
        QTest::newRow("SE, dms, no hemisphere")
                << southEast << QGeoCoordinate::DegreesMinutesSeconds
                << QString("-27%1 28' 3.3\", 153%1 1' 40.4\"").arg(DEGREES_SYMB);
        QTest::newRow("SE, dms, hemisphere")
                << southEast << QGeoCoordinate::DegreesMinutesSecondsWithHemisphere
                << QString("27%1 28' 3.3\" S, 153%1 1' 40.4\" E").arg(DEGREES_SYMB);

        QTest::newRow("SE with alt, dd, no hemisphere, 28.2341m")
                << southEastWithAlt << QGeoCoordinate::Degrees
                << QString("-27.46758%1, 153.02789%1, 28.2341m").arg(DEGREES_SYMB);
        QTest::newRow("SE with alt, dd, hemisphere, 28.2341m")
                << southEastWithAlt << QGeoCoordinate::DegreesWithHemisphere
                << QString("27.46758%1 S, 153.02789%1 E, 28.2341m").arg(DEGREES_SYMB);
        QTest::newRow("SE with alt, dm, no hemisphere, 28.2341m")
                << southEastWithAlt << QGeoCoordinate::DegreesMinutes
                << QString("-27%1 28.055', 153%1 1.674', 28.2341m").arg(DEGREES_SYMB);
        QTest::newRow("SE with alt, dm, hemisphere, 28.2341m")
                << southEastWithAlt << QGeoCoordinate::DegreesMinutesWithHemisphere
                << QString("27%1 28.055' S, 153%1 1.674' E, 28.2341m").arg(DEGREES_SYMB);
        QTest::newRow("SE with alt, dms, no hemisphere, 28.2341m")
                << southEastWithAlt << QGeoCoordinate::DegreesMinutesSeconds
                << QString("-27%1 28' 3.3\", 153%1 1' 40.4\", 28.2341m").arg(DEGREES_SYMB);
        QTest::newRow("SE with alt, dms, hemisphere, 28.2341m")
                << southEastWithAlt << QGeoCoordinate::DegreesMinutesSecondsWithHemisphere
                << QString("27%1 28' 3.3\" S, 153%1 1' 40.4\" E, 28.2341m").arg(DEGREES_SYMB);;

        QTest::newRow("NW, dd, no hemisphere")
                << northWest << QGeoCoordinate::Degrees
                << QString("27.46758%1, -153.02789%1").arg(DEGREES_SYMB);
        QTest::newRow("NW, dd, hemisphere")
                << northWest << QGeoCoordinate::DegreesWithHemisphere
                << QString("27.46758%1 N, 153.02789%1 W").arg(DEGREES_SYMB);
        QTest::newRow("NW, dm, no hemisphere")
                << northWest << QGeoCoordinate::DegreesMinutes
                << QString("27%1 28.055', -153%1 1.674'").arg(DEGREES_SYMB);
        QTest::newRow("NW, dm, hemisphere")
                << northWest << QGeoCoordinate::DegreesMinutesWithHemisphere
                << QString("27%1 28.055' N, 153%1 1.674' W").arg(DEGREES_SYMB);
        QTest::newRow("NW, dms, no hemisphere")
                << northWest << QGeoCoordinate::DegreesMinutesSeconds
                << QString("27%1 28' 3.3\", -153%1 1' 40.4\"").arg(DEGREES_SYMB);
        QTest::newRow("NW, dms, hemisphere")
                << northWest << QGeoCoordinate::DegreesMinutesSecondsWithHemisphere
                << QString("27%1 28' 3.3\" N, 153%1 1' 40.4\" W").arg(DEGREES_SYMB);

        QTest::newRow("NW with alt, dd, no hemisphere, 28.2341m")
                << northWestWithAlt << QGeoCoordinate::Degrees
                << QString("27.46758%1, -153.02789%1, 28.2341m").arg(DEGREES_SYMB);
        QTest::newRow("NW with alt, dd, hemisphere, 28.2341m")
                << northWestWithAlt << QGeoCoordinate::DegreesWithHemisphere
                << QString("27.46758%1 N, 153.02789%1 W, 28.2341m").arg(DEGREES_SYMB);
        QTest::newRow("NW with alt, dm, no hemisphere, 28.2341m")
                << northWestWithAlt << QGeoCoordinate::DegreesMinutes
                << QString("27%1 28.055', -153%1 1.674', 28.2341m").arg(DEGREES_SYMB);
        QTest::newRow("NW with alt, dm, hemisphere, 28.2341m")
                << northWestWithAlt << QGeoCoordinate::DegreesMinutesWithHemisphere
                << QString("27%1 28.055' N, 153%1 1.674' W, 28.2341m").arg(DEGREES_SYMB);
        QTest::newRow("NW with alt, dms, no hemisphere, 28.2341m")
                << northWestWithAlt << QGeoCoordinate::DegreesMinutesSeconds
                << QString("27%1 28' 3.3\", -153%1 1' 40.4\", 28.2341m").arg(DEGREES_SYMB);
        QTest::newRow("NW with alt, dms, hemisphere, 28.2341m")
                << northWestWithAlt << QGeoCoordinate::DegreesMinutesSecondsWithHemisphere
                << QString("27%1 28' 3.3\" N, 153%1 1' 40.4\" W, 28.2341m").arg(DEGREES_SYMB);

        QTest::newRow("SW, dd, no hemisphere")
                << southWest << QGeoCoordinate::Degrees
                << QString("-27.46758%1, -153.02789%1").arg(DEGREES_SYMB);
        QTest::newRow("SW, dd, hemisphere")
                << southWest << QGeoCoordinate::DegreesWithHemisphere
                << QString("27.46758%1 S, 153.02789%1 W").arg(DEGREES_SYMB);
        QTest::newRow("SW, dm, no hemisphere")
                << southWest << QGeoCoordinate::DegreesMinutes
                << QString("-27%1 28.055', -153%1 1.674'").arg(DEGREES_SYMB);
        QTest::newRow("SW, dm, hemisphere")
                << southWest << QGeoCoordinate::DegreesMinutesWithHemisphere
                << QString("27%1 28.055' S, 153%1 1.674' W").arg(DEGREES_SYMB);
        QTest::newRow("SW, dms, no hemisphere")
                << southWest << QGeoCoordinate::DegreesMinutesSeconds
                << QString("-27%1 28' 3.3\", -153%1 1' 40.4\"").arg(DEGREES_SYMB);
        QTest::newRow("SW, dms, hemisphere")
                << southWest << QGeoCoordinate::DegreesMinutesSecondsWithHemisphere
                << QString("27%1 28' 3.3\" S, 153%1 1' 40.4\" W").arg(DEGREES_SYMB);

        QTest::newRow("SW with alt, dd, no hemisphere, 28.2341m")
                << southWestWithAlt << QGeoCoordinate::Degrees
                << QString("-27.46758%1, -153.02789%1, 28.2341m").arg(DEGREES_SYMB);
        QTest::newRow("SW with alt, dd, hemisphere, 28.2341m")
                << southWestWithAlt << QGeoCoordinate::DegreesWithHemisphere
                << QString("27.46758%1 S, 153.02789%1 W, 28.2341m").arg(DEGREES_SYMB);
        QTest::newRow("SW with alt, dm, no hemisphere, 28.2341m")
                << southWestWithAlt << QGeoCoordinate::DegreesMinutes
                << QString("-27%1 28.055', -153%1 1.674', 28.2341m").arg(DEGREES_SYMB);
        QTest::newRow("SW with alt, dm, hemisphere, 28.2341m")
                << southWestWithAlt << QGeoCoordinate::DegreesMinutesWithHemisphere
                << QString("27%1 28.055' S, 153%1 1.674' W, 28.2341m").arg(DEGREES_SYMB);
        QTest::newRow("SW with alt, dms, no hemisphere, 28.2341m")
                << southWestWithAlt << QGeoCoordinate::DegreesMinutesSeconds
                << QString("-27%1 28' 3.3\", -153%1 1' 40.4\", 28.2341m").arg(DEGREES_SYMB);
        QTest::newRow("SW with alt, dms, hemisphere, 28.2341m")
                << southWestWithAlt << QGeoCoordinate::DegreesMinutesSecondsWithHemisphere
                << QString("27%1 28' 3.3\" S, 153%1 1' 40.4\" W, 28.2341m").arg(DEGREES_SYMB);

        QTest::newRow("Wrap seconds to Minutes DMSH")
                << QGeoCoordinate(1.1333333, 1.1333333) << QGeoCoordinate::DegreesMinutesSecondsWithHemisphere
                << QString( "1%1 8' 0.0\" N, 1%1 8' 0.0\" E").arg(DEGREES_SYMB);
        QTest::newRow("Wrap seconds to Minutes DMS")
                << QGeoCoordinate(1.1333333, 1.1333333) << QGeoCoordinate::DegreesMinutesSeconds
                << QString( "1%1 8' 0.0\", 1%1 8' 0.0\"").arg(DEGREES_SYMB);
        QTest::newRow("Wrap minutes to Degrees DMH")
                << QGeoCoordinate(1.999999, 1.999999) << QGeoCoordinate::DegreesMinutesWithHemisphere
                << QString( "2%1 0.000' N, 2%1 0.000' E").arg(DEGREES_SYMB);
        QTest::newRow("Wrap minutes to Degrees DM")
                << QGeoCoordinate(1.999999, 1.999999) << QGeoCoordinate::DegreesMinutes
                << QString( "2%1 0.000', 2%1 0.000'").arg(DEGREES_SYMB);

        QTest::newRow("Wrap seconds to minutes to Degrees DM -> above valid long/lat values")
                << QGeoCoordinate(89.9999, 179.9999) << QGeoCoordinate::DegreesMinutesSeconds
                << QString( "90%1 0' 0.0\", 180%1 0' 0.0\"").arg(DEGREES_SYMB);

        QTest::newRow("Wrap minutes to Degrees DM ->above valid long/lat values")
                << QGeoCoordinate(89.9999, 179.9999) << QGeoCoordinate::DegreesMinutes
                << QString( "90%1 0.000', 180%1 0.000'").arg(DEGREES_SYMB);

    }

    void datastream()
    {
        QFETCH(QGeoCoordinate, c);

        QByteArray ba;
        QDataStream out(&ba, QIODevice::WriteOnly);
        out << c;

        QDataStream in(&ba, QIODevice::ReadOnly);
        QGeoCoordinate inCoord;
        in >> inCoord;
        QCOMPARE(inCoord, c);
    }

    void datastream_data()
    {
        QTest::addColumn<QGeoCoordinate>("c");

        QTest::newRow("invalid") << QGeoCoordinate();
        QTest::newRow("valid lat, long") << BRISBANE;
        QTest::newRow("valid lat, long, alt") << QGeoCoordinate(-1, -1, -1);
        QTest::newRow("valid lat, long, alt again") << QGeoCoordinate(1, 1, 1);
    }

    void debug()
    {
        QFETCH(QGeoCoordinate, c);
        QFETCH(int, nextValue);
        QFETCH(QByteArray, debugString);

        qInstallMessageHandler(tst_qgeocoordinate_messageHandler);
        qDebug() << c << nextValue;
        qInstallMessageHandler(0);
        QCOMPARE(tst_qgeocoordinate_debug, debugString);
    }

    void debug_data()
    {
        QTest::addColumn<QGeoCoordinate>("c");
        QTest::addColumn<int>("nextValue");
        QTest::addColumn<QByteArray>("debugString");

        QTest::newRow("uninitialized") << QGeoCoordinate() << 45
                << QByteArray("QGeoCoordinate(?, ?) 45");
        QTest::newRow("initialized without altitude") << BRISBANE << 45
                << (QString("QGeoCoordinate(%1, %2) 45").arg(BRISBANE.latitude(), 0, 'g', 9)
                        .arg(BRISBANE.longitude(), 0, 'g', 9)).toLatin1();
        QTest::newRow("invalid initialization") << QGeoCoordinate(-100,-200) << 45
                << QByteArray("QGeoCoordinate(?, ?) 45");
        QTest::newRow("initialized with altitude") << QGeoCoordinate(1,2,3) << 45
                << QByteArray("QGeoCoordinate(1, 2, 3) 45");
        QTest::newRow("extra long coordinates") << QGeoCoordinate(89.123412341, 179.123412341)
                << 45 << QByteArray("QGeoCoordinate(89.123412341, 179.12341234) 45");
    }

    void hash()
    {
        uint s1 = qHash(QGeoCoordinate(1, 1, 2));
        uint s2 = qHash(QGeoCoordinate(2, 1, 1));
        uint s3 = qHash(QGeoCoordinate(1, 2, 1));
        uint s10 = qHash(QGeoCoordinate(0, 0, 2));
        uint s20 = qHash(QGeoCoordinate(2, 0, 0));
        uint s30 = qHash(QGeoCoordinate(0, 2, 0));
        uint s30NoAlt = qHash(QGeoCoordinate(0, 2));
        uint s30WithSeed = qHash(QGeoCoordinate(0, 2, 0), 1);
        uint nullCoordinate = qHash(QGeoCoordinate());

        uint north1 = qHash(QGeoCoordinate(90.0, 34.7, 0));
        uint north2 = qHash(QGeoCoordinate(90.0, 180, 0));

        uint south1 = qHash(QGeoCoordinate(90.0, 67.7, 34.0));
        uint south2 = qHash(QGeoCoordinate(90.0, 111, 34.0));

        QVERIFY(s1 != s2);
        QVERIFY(s2 != s3);
        QVERIFY(s1 != s3);
        QVERIFY(s10 != s20);
        QVERIFY(s20 != s30);
        QVERIFY(s10 != s30);
        QVERIFY(s30NoAlt != s30);
        QVERIFY(s30WithSeed != s30);

        QVERIFY(nullCoordinate != s1);
        QVERIFY(nullCoordinate != s2);
        QVERIFY(nullCoordinate != s3);
        QVERIFY(nullCoordinate != s10);
        QVERIFY(nullCoordinate != s20);
        QVERIFY(nullCoordinate != s30);
        QVERIFY(nullCoordinate != s30NoAlt);
        QVERIFY(nullCoordinate != s30WithSeed);

        QVERIFY(north1 == north2);
        QVERIFY(south1 == south2);
    }
};

QTEST_GUILESS_MAIN(tst_QGeoCoordinate)
#include "tst_qgeocoordinate.moc"
