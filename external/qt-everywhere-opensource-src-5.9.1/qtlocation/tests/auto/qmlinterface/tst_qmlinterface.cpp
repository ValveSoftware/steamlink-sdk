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

#include <QtTest/QtTest>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlListReference>
#include <QtPositioning/QGeoCoordinate>
#include <QtPositioning/QGeoAddress>
#include <QtPositioning/QGeoRectangle>
#include <QtPositioning/QGeoCircle>
#include <QtPositioning/QGeoLocation>
#include <QtLocation/QPlaceCategory>
#include <QtLocation/QPlace>
#include <QtLocation/QPlaceIcon>
#include <QtLocation/QPlaceRatings>
#include <QtLocation/QPlaceSupplier>
#include <QtLocation/QPlaceUser>
#include <QtLocation/QPlaceAttribute>
#include <QtLocation/QPlaceContactDetail>

class tst_qmlinterface : public QObject
{
    Q_OBJECT

public:
    tst_qmlinterface();

private Q_SLOTS:
    void testAddress();
    void testLocation();
    void testCategory();
    void testIcon();
    void testRatings();
    void testSupplier();
    void testUser();
    void testPlaceAttribute();
    void testContactDetail();
    void testPlace();

private:
    QGeoCoordinate m_coordinate;
    QGeoAddress m_address;
    QGeoRectangle m_rectangle;
    QGeoLocation m_location;
    QPlaceCategory m_category;
    QPlaceIcon m_icon;
    QPlaceRatings m_ratings;
    QPlaceSupplier m_supplier;
    QPlaceUser m_user;
    QPlaceAttribute m_placeAttribute;
    QPlaceContactDetail m_contactDetail;
    QList<QPlaceCategory> m_categories;
    QPlace m_place;
};

tst_qmlinterface::tst_qmlinterface()
{
    m_coordinate.setLongitude(10.0);
    m_coordinate.setLatitude(20.0);
    m_coordinate.setAltitude(30.0);

    m_address.setCity(QStringLiteral("Brisbane"));
    m_address.setCountry(QStringLiteral("Australia"));
    m_address.setCountryCode(QStringLiteral("AU"));
    m_address.setPostalCode(QStringLiteral("4000"));
    m_address.setState(QStringLiteral("Queensland"));
    m_address.setStreet(QStringLiteral("123 Fake Street"));

    m_rectangle.setCenter(m_coordinate);
    m_rectangle.setHeight(30.0);
    m_rectangle.setWidth(40.0);

    m_location.setAddress(m_address);
    m_location.setBoundingBox(m_rectangle);
    m_location.setCoordinate(m_coordinate);

    m_category.setName(QStringLiteral("Test category"));
    m_category.setCategoryId(QStringLiteral("test-category-id"));

    QVariantMap iconParams;
    iconParams.insert(QPlaceIcon::SingleUrl, QUrl(QStringLiteral("http://www.example.com/test-icon.png")));
    m_icon.setParameters(iconParams);

    m_ratings.setAverage(3.5);
    m_ratings.setMaximum(5.0);
    m_ratings.setCount(10);

    m_supplier.setName(QStringLiteral("Test supplier"));
    m_supplier.setUrl(QUrl(QStringLiteral("http://www.example.com/test-supplier")));
    m_supplier.setSupplierId(QStringLiteral("test-supplier-id"));
    m_supplier.setIcon(m_icon);

    m_user.setName(QStringLiteral("Test User"));
    m_user.setUserId(QStringLiteral("test-user-id"));

    m_placeAttribute.setLabel(QStringLiteral("Test Attribute"));
    m_placeAttribute.setText(QStringLiteral("Test attribute text"));

    m_contactDetail.setLabel(QStringLiteral("Test Contact Detail"));
    m_contactDetail.setValue(QStringLiteral("Test contact detail value"));

    QPlaceCategory category;
    category.setName(QStringLiteral("Test category 1"));
    category.setCategoryId(QStringLiteral("test-category-id-1"));
    m_categories.append(category);
    category.setName(QStringLiteral("Test category 2"));
    category.setCategoryId(QStringLiteral("test-category-id-2"));
    m_categories.append(category);

    m_place.setName(QStringLiteral("Test Place"));
    m_place.setPlaceId(QStringLiteral("test-place-id"));
    m_place.setAttribution(QStringLiteral("Place data by Foo"));
    m_place.setCategories(m_categories);
    m_place.setLocation(m_location);
    m_place.setRatings(m_ratings);
    m_place.setIcon(m_icon);
    m_place.setSupplier(m_supplier);
    m_place.setVisibility(QLocation::PrivateVisibility);
}

void tst_qmlinterface::testAddress()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, SRCDIR "data/TestAddress.qml");
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QObject *qmlObject = component.create();

    QGeoAddress address = qmlObject->property("address").value<QGeoAddress>();

    QCOMPARE(address, m_address);

    qmlObject->setProperty("address", QVariant::fromValue(QGeoAddress()));

    QVERIFY(qmlObject->property("city").toString().isEmpty());
    QVERIFY(qmlObject->property("country").toString().isEmpty());
    QVERIFY(qmlObject->property("countryCode").toString().isEmpty());
    QVERIFY(qmlObject->property("postalCode").toString().isEmpty());
    QVERIFY(qmlObject->property("state").toString().isEmpty());
    QVERIFY(qmlObject->property("street").toString().isEmpty());

    delete qmlObject;
}

void tst_qmlinterface::testLocation()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, SRCDIR "data/TestLocation.qml");
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QObject *qmlObject = component.create();

    QGeoLocation location = qmlObject->property("location").value<QGeoLocation>();

    QCOMPARE(location, m_location);

    qmlObject->setProperty("location", QVariant::fromValue(QGeoLocation()));

    QCOMPARE(qmlObject->property("address").value<QGeoAddress>(), QGeoAddress());
    QCOMPARE(qmlObject->property("boundingBox").value<QGeoRectangle>(), QGeoRectangle());
    QCOMPARE(qmlObject->property("coordinate").value<QGeoCoordinate>(), QGeoCoordinate());

    delete qmlObject;
}

void tst_qmlinterface::testCategory()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, SRCDIR "data/TestCategory.qml");
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QObject *qmlObject = component.create();

    QPlaceCategory category = qmlObject->property("category").value<QPlaceCategory>();

    QCOMPARE(category, m_category);

    qmlObject->setProperty("category", QVariant::fromValue(QPlaceCategory()));

    QVERIFY(qmlObject->property("name").toString().isEmpty());
    QVERIFY(qmlObject->property("categoryId").toString().isEmpty());

    delete qmlObject;
}

void tst_qmlinterface::testIcon()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, SRCDIR "data/TestIcon.qml");
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QObject *qmlObject = component.create();

    QPlaceIcon icon = qmlObject->property("icon").value<QPlaceIcon>();

    QCOMPARE(icon, m_icon);

    qmlObject->setProperty("icon", QVariant::fromValue(QPlaceIcon()));

    QVERIFY(!qmlObject->property("fullUrl").toUrl().isValid());

    delete qmlObject;
}

void tst_qmlinterface::testRatings()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, SRCDIR "data/TestRatings.qml");
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QObject *qmlObject = component.create();

    QPlaceRatings ratings = qmlObject->property("ratings").value<QPlaceRatings>();

    QCOMPARE(ratings, m_ratings);

    qmlObject->setProperty("ratings", QVariant::fromValue(QPlaceRatings()));

    QCOMPARE(qmlObject->property("average").value<qreal>(), 0.0);
    QCOMPARE(qmlObject->property("maximum").value<qreal>(), 0.0);
    QCOMPARE(qmlObject->property("average").toInt(), 0);

    delete qmlObject;
}

void tst_qmlinterface::testSupplier()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, SRCDIR "data/TestSupplier.qml");
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QObject *qmlObject = component.create();

    QPlaceSupplier supplier = qmlObject->property("supplier").value<QPlaceSupplier>();

    QCOMPARE(supplier, m_supplier);

    qmlObject->setProperty("supplier", QVariant::fromValue(QPlaceSupplier()));

    QVERIFY(qmlObject->property("name").toString().isEmpty());
    QVERIFY(!qmlObject->property("url").toUrl().isValid());
    QVERIFY(qmlObject->property("supplierId").toString().isEmpty());
    QCOMPARE(qmlObject->property("icon").value<QPlaceIcon>(), QPlaceIcon());

    delete qmlObject;
}

void tst_qmlinterface::testUser()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, SRCDIR "data/TestUser.qml");
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QObject *qmlObject = component.create();

    QPlaceUser user = qmlObject->property("user").value<QPlaceUser>();

    QCOMPARE(user, m_user);

    qmlObject->setProperty("user", QVariant::fromValue(QPlaceUser()));

    QVERIFY(qmlObject->property("name").toString().isEmpty());
    QVERIFY(qmlObject->property("userId").toString().isEmpty());

    delete qmlObject;
}

void tst_qmlinterface::testPlaceAttribute()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, SRCDIR "data/TestPlaceAttribute.qml");
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QObject *qmlObject = component.create();

    QPlaceAttribute placeAttribute = qmlObject->property("attribute").value<QPlaceAttribute>();

    QCOMPARE(placeAttribute, m_placeAttribute);

    qmlObject->setProperty("attribute", QVariant::fromValue(QPlaceAttribute()));

    QVERIFY(qmlObject->property("label").toString().isEmpty());
    QVERIFY(qmlObject->property("text").toString().isEmpty());

    delete qmlObject;
}

void tst_qmlinterface::testContactDetail()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, SRCDIR "data/TestContactDetail.qml");
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QObject *qmlObject = component.create();

    QPlaceContactDetail contactDetail = qmlObject->property("contactDetail").value<QPlaceContactDetail>();

    QCOMPARE(contactDetail, m_contactDetail);

    qmlObject->setProperty("contactDetail", QVariant::fromValue(QPlaceContactDetail()));

    QVERIFY(qmlObject->property("label").toString().isEmpty());
    QVERIFY(qmlObject->property("value").toString().isEmpty());

    delete qmlObject;
}

void tst_qmlinterface::testPlace()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, SRCDIR "data/TestPlace.qml");
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QObject *qmlObject = component.create();

    QPlace place = qmlObject->property("place").value<QPlace>();

    QCOMPARE(place, m_place);

    qmlObject->setProperty("place", QVariant::fromValue(QPlace()));

    QVERIFY(qmlObject->property("name").toString().isEmpty());
    QVERIFY(qmlObject->property("placeId").toString().isEmpty());
    QVERIFY(qmlObject->property("attribution").toString().isEmpty());
    QQmlListReference categories(qmlObject, "categories", &engine);
    QCOMPARE(categories.count(), 0);
    QCOMPARE(qmlObject->property("location").value<QGeoLocation>(), QGeoLocation());
    QCOMPARE(qmlObject->property("ratings").value<QPlaceRatings>(), QPlaceRatings());
    QCOMPARE(qmlObject->property("icon").value<QPlaceIcon>(), QPlaceIcon());
    QCOMPARE(qmlObject->property("supplier").value<QPlaceSupplier>(), QPlaceSupplier());

    delete qmlObject;
}

QTEST_MAIN(tst_qmlinterface)

#include "tst_qmlinterface.moc"
