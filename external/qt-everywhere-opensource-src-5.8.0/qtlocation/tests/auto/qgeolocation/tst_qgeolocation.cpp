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

#include "tst_qgeolocation.h"

QT_USE_NAMESPACE

tst_QGeoLocation::tst_QGeoLocation()
{
}

void tst_QGeoLocation::initTestCase()
{

}

void tst_QGeoLocation::cleanupTestCase()
{

}

void tst_QGeoLocation::init()
{
}

void tst_QGeoLocation::cleanup()
{
}

void tst_QGeoLocation::constructor()
{
    QCOMPARE(m_location.address(), m_address);
    QCOMPARE(m_location.coordinate(), m_coordinate);
    QCOMPARE(m_location.boundingBox(), m_viewport);
}

void tst_QGeoLocation::copy_constructor()
{
    QGeoLocation *qgeolocationcopy = new QGeoLocation (m_location);
    QCOMPARE(m_location, *qgeolocationcopy);
    delete qgeolocationcopy;
}

void tst_QGeoLocation::destructor()
{
    QGeoLocation *qgeolocationcopy;

    qgeolocationcopy = new QGeoLocation();
    delete qgeolocationcopy;

    qgeolocationcopy = new QGeoLocation(m_location);
    delete qgeolocationcopy;
}

void tst_QGeoLocation::address()
{
    m_address.setCity("Berlin");
    m_address.setCountry("Germany");
    m_address.setCountryCode("DEU");
    m_address.setDistrict("Mitte");
    m_address.setPostalCode("10115");
    m_address.setStreet("Invalidenstrasse");

    m_location.setAddress(m_address);

    QCOMPARE(m_location.address(),m_address);

    m_address.setPostalCode("10125");
    QVERIFY(m_location.address() != m_address);
}

void tst_QGeoLocation::coordinate()
{
    m_coordinate.setLatitude(13.3851);
    m_coordinate.setLongitude(52.5312);
    m_coordinate.setAltitude(134.23);

    m_location.setCoordinate(m_coordinate);

    QCOMPARE(m_location.coordinate(), m_coordinate);

    m_coordinate.setAltitude(0);
    QVERIFY(m_location.coordinate() != m_coordinate);
}

void tst_QGeoLocation::viewport()
{
    m_coordinate.setLatitude(13.3851);
    m_coordinate.setLongitude(52.5312);

    QGeoRectangle qgeoboundingboxcopy(m_coordinate, 0.4, 0.4);
    m_location.setBoundingBox(qgeoboundingboxcopy);

    QCOMPARE(m_location.boundingBox(),qgeoboundingboxcopy);

    qgeoboundingboxcopy.setHeight(1);

    QVERIFY(m_location.boundingBox() != qgeoboundingboxcopy);
}

void tst_QGeoLocation::operators()
{
    QGeoAddress qgeoaddresscopy;
    qgeoaddresscopy.setCity("Berlin");
    qgeoaddresscopy.setCountry("Germany");
    qgeoaddresscopy.setCountryCode("DEU");

    QGeoCoordinate qgeocoordinatecopy (32.324 , 41.324 , 24.55);

    m_address.setCity("Madrid");
    m_address.setCountry("Spain");
    m_address.setCountryCode("SPA");

    m_coordinate.setLatitude(21.3434);
    m_coordinate.setLongitude(38.43443);
    m_coordinate.setAltitude(634.21);

    m_location.setAddress(m_address);
    m_location.setCoordinate(m_coordinate);

    //Create a copy and see that they are the same
    QGeoLocation qgeolocationcopy(m_location);
    QVERIFY(m_location == qgeolocationcopy);
    QVERIFY(!(m_location != qgeolocationcopy));

    //Modify one and test if they are different
   qgeolocationcopy.setAddress(qgeoaddresscopy);
   QVERIFY(!(m_location == qgeolocationcopy));
   QVERIFY(m_location != qgeolocationcopy);
   qgeolocationcopy.setCoordinate(qgeocoordinatecopy);
   QVERIFY(!(m_location == qgeolocationcopy));
   QVERIFY(m_location != qgeolocationcopy);

    //delete qgeolocationcopy;
    //Asign and test that they are the same
    qgeolocationcopy = m_location;
    QVERIFY(m_location ==qgeolocationcopy);
    QVERIFY(!(m_location != qgeolocationcopy));
}

void tst_QGeoLocation::comparison()
{
    QFETCH(QString, dataField);

    QGeoLocation location;

    //set address
    QGeoAddress address;
    address.setStreet("21 jump st");
    address.setCountry("USA");
    location.setAddress(address);

    //set coordinate
    location.setCoordinate(QGeoCoordinate(5,10));

    //set viewport
    location.setBoundingBox(QGeoRectangle(QGeoCoordinate(5,5),0.4,0.4));

    QGeoLocation otherLocation(location);

    if (dataField == "no change") {
        QCOMPARE(location, otherLocation);
    } else {
        if (dataField == "address") {
            QGeoAddress otherAddress;
            otherAddress.setStreet("42 evergreen tce");
            otherAddress.setCountry("USA");
            otherLocation.setAddress(otherAddress);
        } else if (dataField == "coordinate") {
            otherLocation.setCoordinate(QGeoCoordinate(12,13));
        } else if (dataField == "viewport"){
            otherLocation.setBoundingBox(QGeoRectangle(QGeoCoordinate(1,2), 0.5,0.5));
        } else {
            qFatal("Unknown data field to test");
        }

        QVERIFY(location != otherLocation);
    }
}

void tst_QGeoLocation::comparison_data()
{
    QTest::addColumn<QString> ("dataField");
    QTest::newRow("no change") << "no change";
    QTest::newRow("address") << "address";
    QTest::newRow("coordinate") << "coordinate";
}

void tst_QGeoLocation::isEmpty()
{
    QGeoAddress address;
    address.setCity(QStringLiteral("Braunschweig"));
    QVERIFY(!address.isEmpty());

    QGeoRectangle boundingBox;
    boundingBox.setTopLeft(QGeoCoordinate(1, -1));
    boundingBox.setBottomRight(QGeoCoordinate(-1, 1));
    QVERIFY(!boundingBox.isEmpty());

    QGeoLocation location;

    QVERIFY(location.isEmpty());

    // address
    location.setAddress(address);
    QVERIFY(!location.isEmpty());
    location.setAddress(QGeoAddress());
    QVERIFY(location.isEmpty());

    // coordinate
    location.setCoordinate(QGeoCoordinate(1, 2));
    QVERIFY(!location.isEmpty());
    location.setCoordinate(QGeoCoordinate());
    QVERIFY(location.isEmpty());

    // bounding box
    location.setBoundingBox(boundingBox);
    QVERIFY(!location.isEmpty());
    location.setBoundingBox(QGeoRectangle());
    QVERIFY(location.isEmpty());
}

QTEST_APPLESS_MAIN(tst_QGeoLocation);

