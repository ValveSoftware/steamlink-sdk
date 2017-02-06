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

#include <QtCore/QString>
#include <QtTest/QtTest>

#include <qplace.h>
#include <qplaceimage.h>
#include <qplaceattribute.h>
#include <QtLocation/QPlaceEditorial>

QT_USE_NAMESPACE

class tst_Place : public QObject
{
    Q_OBJECT

public:
    tst_Place();

private Q_SLOTS:
    void constructorTest();
    void categoriesTest();
    void detailsFetchedTest();
    void locationTest();
    void ratingTest();
    void supplierTest();
    void imageContentTest();
    void reviewContentTest();
    void editorialContentTest();
    void totalContentCountTest();
    void totalContentCountTest_data();
    void nameTest();
    void placeIdTest();
    void attributionTest();
    void contactDetailsTest();
    void primaryPhoneTest();
    void primaryFaxTest();
    void primaryEmailTest();
    void primaryWebsiteTest();
    void operatorsTest();
    void extendedAttributeTest();
    void visibilityTest();
    void isEmptyTest();
};

tst_Place::tst_Place()
{
}

void tst_Place::constructorTest()
{
    QPlace testObj;
    testObj.setPlaceId("testId");
    QPlaceAttribute paymentMethods;
    paymentMethods.setLabel("Payment methods");
    paymentMethods.setText("Visa");
    testObj.setExtendedAttribute(QStringLiteral("paymentMethods"), paymentMethods);
    QGeoLocation loc;
    loc.setCoordinate(QGeoCoordinate(10,20));
    testObj.setLocation(loc);
    QPlace *testObjPtr = new QPlace(testObj);

    QVERIFY2(testObjPtr != NULL, "Copy constructor - null");
    QVERIFY2(*testObjPtr == testObj, "Copy constructor - compare");

    delete testObjPtr;
}

void tst_Place::nameTest()
{
    QPlace testObj;
    QVERIFY2(testObj.name() == QString(), "Wrong default value");
    testObj.setName("testText");
    QVERIFY2(testObj.name() == "testText", "Wrong value returned");
}

void tst_Place::placeIdTest()
{
    QPlace testObj;
    QVERIFY2(testObj.placeId() == QString(), "Wrong default value");
    testObj.setPlaceId("testText");
    QVERIFY2(testObj.placeId() == "testText", "Wrong value returned");
}

void tst_Place::totalContentCountTest()
{
    QFETCH(QPlaceContent::Type, contentType);
    QPlace testObj;
    QVERIFY2(testObj.totalContentCount(contentType) == 0, "Wrong default value");
    testObj.setTotalContentCount(contentType, 50);
    QVERIFY2(testObj.totalContentCount(contentType) == 50, "Wrong value returned");

    testObj.setTotalContentCount(contentType,0);
    QVERIFY2(testObj.totalContentCount(contentType) == 0, "Wrong value returned");
}

void tst_Place::totalContentCountTest_data()
{
    QTest::addColumn<QPlaceContent::Type>("contentType");
    QTest::newRow("Image content") << QPlaceContent::ImageType;
    QTest::newRow("Editoral content") << QPlaceContent::EditorialType;
    QTest::newRow("Review content") << QPlaceContent::ReviewType;
}

void tst_Place::ratingTest()
{
    QPlace testObj;
    QVERIFY2(testObj.ratings() == QPlaceRatings(), "Wrong default value");
    QPlaceRatings obj;
    obj.setCount(10);
    testObj.setRatings(obj);
    QVERIFY2(testObj.ratings() == obj, "Wrong value returned");
}

void tst_Place::locationTest()
{
    QPlace testObj;
    QVERIFY2(testObj.location() == QGeoLocation(), "Wrong default value");
    QGeoLocation obj;
    obj.setCoordinate(QGeoCoordinate(10,20));
    testObj.setLocation(obj);
    QVERIFY2(testObj.location() == obj, "Wrong value returned");
}

void tst_Place::detailsFetchedTest()
{
    QPlace testPlace;
    QVERIFY2(testPlace.detailsFetched() == false, "Wrong default value");
    testPlace.setDetailsFetched(true);
    QVERIFY2(testPlace.detailsFetched() == true, "Wrong value returned");
    testPlace.setDetailsFetched(false);
    QVERIFY2(testPlace.detailsFetched() == false, "Wrong value returned");
}

void tst_Place::imageContentTest()
{
    QPlace place;
    QVERIFY2(place.content(QPlaceContent::ImageType).count() ==0,"Wrong default value");

    QPlaceImage dummyImage;
    dummyImage.setUrl(QUrl("www.dummy.one"));

    QPlaceImage dummyImage2;
    dummyImage2.setUrl(QUrl("www.dummy.two"));

    QPlaceImage dummyImage3;
    dummyImage3.setUrl(QUrl("www.dummy.three"));

    QPlaceContent::Collection imageCollection;
    imageCollection.insert(0,dummyImage);
    imageCollection.insert(1, dummyImage2);
    imageCollection.insert(2, dummyImage3);

    place.setContent(QPlaceContent::ImageType, imageCollection);
    QPlaceContent::Collection retrievedCollection = place.content(QPlaceContent::ImageType);

    QCOMPARE(retrievedCollection.count(), 3);
    QCOMPARE(QPlaceImage(retrievedCollection.value(0)), dummyImage);
    QCOMPARE(QPlaceImage(retrievedCollection.value(1)), dummyImage2);
    QCOMPARE(QPlaceImage(retrievedCollection.value(2)), dummyImage3);

    //replace the second and insert a sixth image
    //indexes 4 and 5 are "missing"
    QPlaceImage dummyImage2New;
    dummyImage2.setUrl(QUrl("www.dummy.two.new"));

    QPlaceImage dummyImage6;
    dummyImage6.setUrl(QUrl("www.dummy.six"));

    imageCollection.clear();
    imageCollection.insert(1, dummyImage2New);
    imageCollection.insert(5, dummyImage6);
    place.insertContent(QPlaceContent::ImageType, imageCollection);

    retrievedCollection = place.content(QPlaceContent::ImageType);
    QCOMPARE(retrievedCollection.count(), 4);
    QCOMPARE(QPlaceImage(retrievedCollection.value(0)), dummyImage);
    QCOMPARE(QPlaceImage(retrievedCollection.value(1)), dummyImage2New);
    QCOMPARE(QPlaceImage(retrievedCollection.value(2)), dummyImage3);
    QCOMPARE(QPlaceImage(retrievedCollection.value(3)), QPlaceImage());
    QCOMPARE(QPlaceImage(retrievedCollection.value(4)), QPlaceImage());
    QCOMPARE(QPlaceImage(retrievedCollection.value(5)), dummyImage6);
}

void tst_Place::reviewContentTest()
{
    QPlace place;
    QVERIFY2(place.content(QPlaceContent::ReviewType).count() ==0,"Wrong default value");

    QPlaceReview dummyReview;
    dummyReview.setTitle(QStringLiteral("Review 1"));

    QPlaceReview dummyReview2;
    dummyReview2.setTitle(QStringLiteral("Review 2"));

    QPlaceReview dummyReview3;
    dummyReview3.setTitle(QStringLiteral("Review 3"));

    QPlaceContent::Collection reviewCollection;
    reviewCollection.insert(0,dummyReview);
    reviewCollection.insert(1, dummyReview2);
    reviewCollection.insert(2, dummyReview3);

    place.setContent(QPlaceContent::ReviewType, reviewCollection);
    QPlaceContent::Collection retrievedCollection = place.content(QPlaceContent::ReviewType);

    QCOMPARE(retrievedCollection.count(), 3);
    QCOMPARE(QPlaceReview(retrievedCollection.value(0)), dummyReview);
    QCOMPARE(QPlaceReview(retrievedCollection.value(1)), dummyReview2);
    QCOMPARE(QPlaceReview(retrievedCollection.value(2)), dummyReview3);

    //replace the second and insert a sixth review
    //indexes 4 and 5 are "missing"
    QPlaceReview dummyReview2New;
    dummyReview2.setTitle(QStringLiteral("Review 2 new"));

    QPlaceReview dummyReview6;
    dummyReview6.setTitle(QStringLiteral("Review 6"));

    reviewCollection.clear();
    reviewCollection.insert(1, dummyReview2New);
    reviewCollection.insert(5, dummyReview6);
    place.insertContent(QPlaceContent::ReviewType, reviewCollection);

    retrievedCollection = place.content(QPlaceContent::ReviewType);
    QCOMPARE(retrievedCollection.count(), 4);
    QCOMPARE(QPlaceReview(retrievedCollection.value(0)), dummyReview);
    QCOMPARE(QPlaceReview(retrievedCollection.value(1)), dummyReview2New);
    QCOMPARE(QPlaceReview(retrievedCollection.value(2)), dummyReview3);
    QCOMPARE(QPlaceReview(retrievedCollection.value(3)), QPlaceReview());
    QCOMPARE(QPlaceReview(retrievedCollection.value(4)), QPlaceReview());
    QCOMPARE(QPlaceReview(retrievedCollection.value(5)), dummyReview6);
}

void tst_Place::editorialContentTest()
{
    QPlace place;
    QVERIFY2(place.content(QPlaceContent::EditorialType).count() == 0, "Wrong default value");

    QPlaceEditorial dummyEditorial;
    dummyEditorial.setTitle(QStringLiteral("Editorial 1"));

    QPlaceEditorial dummyEditorial2;
    dummyEditorial2.setTitle(QStringLiteral("Editorial 2"));

    QPlaceEditorial dummyEditorial3;
    dummyEditorial3.setTitle(QStringLiteral("Editorial 3"));

    QPlaceContent::Collection editorialCollection;
    editorialCollection.insert(0,dummyEditorial);
    editorialCollection.insert(1, dummyEditorial2);
    editorialCollection.insert(2, dummyEditorial3);

    place.setContent(QPlaceContent::EditorialType, editorialCollection);
    QPlaceContent::Collection retrievedCollection = place.content(QPlaceContent::EditorialType);

    QCOMPARE(retrievedCollection.count(), 3);
    QCOMPARE(QPlaceEditorial(retrievedCollection.value(0)), dummyEditorial);
    QCOMPARE(QPlaceEditorial(retrievedCollection.value(1)), dummyEditorial2);
    QCOMPARE(QPlaceEditorial(retrievedCollection.value(2)), dummyEditorial3);

    //replace the second and insert a sixth editorial
    //indexes 4 and 5 are "missing"
    QPlaceEditorial dummyEditorial2New;
    dummyEditorial2.setTitle(QStringLiteral("Editorial 2 new"));

    QPlaceEditorial dummyEditorial6;
    dummyEditorial6.setTitle(QStringLiteral("Editorial 6"));

    editorialCollection.clear();
    editorialCollection.insert(1, dummyEditorial2New);
    editorialCollection.insert(5, dummyEditorial6);
    place.insertContent(QPlaceContent::EditorialType, editorialCollection);

    retrievedCollection = place.content(QPlaceContent::EditorialType);
    QCOMPARE(retrievedCollection.count(), 4);
    QCOMPARE(QPlaceEditorial(retrievedCollection.value(0)), dummyEditorial);
    QCOMPARE(QPlaceEditorial(retrievedCollection.value(1)), dummyEditorial2New);
    QCOMPARE(QPlaceEditorial(retrievedCollection.value(2)), dummyEditorial3);
    QCOMPARE(QPlaceEditorial(retrievedCollection.value(3)), QPlaceEditorial());
    QCOMPARE(QPlaceEditorial(retrievedCollection.value(4)), QPlaceEditorial());
    QCOMPARE(QPlaceEditorial(retrievedCollection.value(5)), dummyEditorial6);
}

void tst_Place::categoriesTest()
{
    QPlace place;
    QVERIFY(place.categories().isEmpty());

    //set a single category
    QPlaceCategory cat1;
    cat1.setName("cat1");

    place.setCategory(cat1);
    QCOMPARE(place.categories().count(), 1);
    QCOMPARE(place.categories().at(0), cat1);

    //set multiple categories
    QPlaceCategory cat2;
    cat2.setName("cat2");

    QPlaceCategory cat3;
    cat3.setName("cat3");

    QList<QPlaceCategory> categories;
    categories << cat2 << cat3;

    place.setCategories(categories);
    QCOMPARE(place.categories().count(), 2);
    QVERIFY(place.categories().contains(cat2));
    QVERIFY(place.categories().contains(cat3));

    //set a single category again while there are multiple categories already assigned.
    place.setCategory(cat1);
    QCOMPARE(place.categories().count(), 1);
    QCOMPARE(place.categories().at(0), cat1);

    //set an empty list of categories
    place.setCategories(QList<QPlaceCategory>());
    QVERIFY(place.categories().isEmpty());
}

void tst_Place::supplierTest()
{
    QPlace testObj;
    QCOMPARE(testObj.supplier(), QPlaceSupplier());

    QPlaceSupplier sup;
    sup.setName("testName1");
    sup.setSupplierId("testId");

    testObj.setSupplier(sup);

    QCOMPARE(testObj.supplier(), sup);
}

void tst_Place::attributionTest()
{
    QPlace testPlace;
    QVERIFY(testPlace.attribution().isEmpty());
    testPlace.setAttribution(QStringLiteral("attribution"));
    QCOMPARE(testPlace.attribution(), QStringLiteral("attribution"));
    testPlace.setAttribution(QString());
    QVERIFY(testPlace.attribution().isEmpty());
}

void tst_Place::contactDetailsTest()
{
    QPlaceContactDetail phone1;
    phone1.setLabel("Phone1");
    phone1.setValue("555-5555");

    QPlaceContactDetail phone2;
    phone2.setLabel("Phone2");
    phone2.setValue("555-5556");

    QList<QPlaceContactDetail> phones;
    phones << phone1 << phone2;


    QPlaceContactDetail email;
    email.setLabel("Email");
    email.setValue("email@email.com");

    QPlace place;
    place.setContactDetails(QPlaceContactDetail::Phone,phones);
    QCOMPARE(place.contactTypes().count(), 1);
    QVERIFY(place.contactTypes().contains(QPlaceContactDetail::Phone));
    QCOMPARE(place.contactDetails(QPlaceContactDetail::Phone), phones);

    place.appendContactDetail(QPlaceContactDetail::Email, email);
    QCOMPARE(place.contactTypes().count(), 2);
    QVERIFY(place.contactTypes().contains(QPlaceContactDetail::Phone));
    QVERIFY(place.contactTypes().contains(QPlaceContactDetail::Email));
    QCOMPARE(place.contactDetails(QPlaceContactDetail::Phone), phones);
    QCOMPARE(place.contactDetails(QPlaceContactDetail::Email).count(), 1);
    QCOMPARE(place.contactDetails(QPlaceContactDetail::Email).at(0), email);

    place.removeContactDetails(QPlaceContactDetail::Phone);
    QCOMPARE(place.contactTypes().count(), 1);
    QVERIFY(!place.contactTypes().contains(QPlaceContactDetail::Phone));
    QVERIFY(place.contactDetails(QPlaceContactDetail::Phone).isEmpty());
    QVERIFY(place.contactTypes().contains(QPlaceContactDetail::Email));
    QCOMPARE(place.contactDetails(QPlaceContactDetail::Email).count(), 1);
    QCOMPARE(place.contactDetails(QPlaceContactDetail::Email).at(0), email);

    place.removeContactDetails(QPlaceContactDetail::Email);
    QVERIFY(place.contactTypes().isEmpty());
    QVERIFY(place.contactDetails(QPlaceContactDetail::Email).isEmpty());
}

void tst_Place::primaryPhoneTest()
{
    QPlace place;
    QVERIFY2(place.primaryPhone().isEmpty(), "Wrong default value");

    QPlaceContactDetail contactDetail;
    contactDetail.setLabel(QStringLiteral("Phone"));
    contactDetail.setValue(QStringLiteral("555-5555"));
    place.appendContactDetail(QPlaceContactDetail::Phone, contactDetail);

    QCOMPARE(place.primaryPhone(), QString("555-5555"));

    //try clearing the primary phone number
    place.setContactDetails(QPlaceContactDetail::Phone, QList<QPlaceContactDetail>());
    QCOMPARE(place.primaryPhone(), QString());
}

void tst_Place::primaryEmailTest()
{
    QPlace place;
    QVERIFY2(place.primaryEmail().isEmpty(), "Wrong default value");

    QPlaceContactDetail contactDetail;
    contactDetail.setLabel(QStringLiteral("Email"));
    contactDetail.setValue(QStringLiteral("test@test.com"));
    place.appendContactDetail(QPlaceContactDetail::Email, contactDetail);

    QCOMPARE(place.primaryEmail(), QStringLiteral("test@test.com"));

    //try clearing the primary email address
    place.setContactDetails(QPlaceContactDetail::Email, QList<QPlaceContactDetail>());
    QCOMPARE(place.primaryEmail(), QString());
}

void tst_Place::primaryFaxTest()
{
    QPlace place;
    QVERIFY2(place.primaryFax().isEmpty(), "Wrong default value");

    QPlaceContactDetail contactDetail;
    contactDetail.setLabel(QStringLiteral("Fax"));
    contactDetail.setValue(QStringLiteral("555-5555"));
    place.appendContactDetail(QPlaceContactDetail::Fax, contactDetail);

    QCOMPARE(place.primaryFax(), QStringLiteral("555-5555"));

    //try clearing the primary fax number
    place.setContactDetails(QPlaceContactDetail::Fax, QList<QPlaceContactDetail>());
    QCOMPARE(place.primaryFax(), QString());
}

void tst_Place::primaryWebsiteTest()
{
    QPlace place;
    QVERIFY2(place.primaryWebsite().isEmpty(), "Wrong default value");

    QPlaceContactDetail contactDetail;
    contactDetail.setLabel(QStringLiteral("Website"));
    contactDetail.setValue(QStringLiteral("www.example.com"));
    place.appendContactDetail(QPlaceContactDetail::Website, contactDetail);

    QCOMPARE(place.primaryWebsite(), QUrl("www.example.com"));

    //try clearing the primary website number
    place.setContactDetails(QPlaceContactDetail::Website, QList<QPlaceContactDetail>());
    QCOMPARE(place.primaryWebsite(), QUrl());
}

void tst_Place::operatorsTest()
{
    QPlace testObj;
    testObj.setPlaceId("testId");
    QPlaceAttribute paymentMethods;
    paymentMethods.setLabel("Payment methods");
    paymentMethods.setText("Visa");
    testObj.setExtendedAttribute(QStringLiteral("paymentMethods"), paymentMethods);
    QGeoLocation loc;
    loc.setCoordinate(QGeoCoordinate(10,20));
    testObj.setLocation(loc);

    QPlace testObj2;
    testObj2 = testObj;
    QVERIFY2(testObj == testObj2, "Not copied correctly");
    testObj2.setPlaceId("342-456");
    QVERIFY2(testObj != testObj2, "Object should be different");
}

void tst_Place::extendedAttributeTest()
{
    QPlace place;
    QVERIFY(place.extendedAttributeTypes().isEmpty());
    QPlaceAttribute smoking;
    smoking.setLabel(QStringLiteral("Public Smoking"));
    smoking.setText(QStringLiteral("No"));

    //test setting of an attribue
    place.setExtendedAttribute(QStringLiteral("smoking"), smoking);

    QVERIFY(place.extendedAttributeTypes().contains(QStringLiteral("smoking")));
    QCOMPARE(place.extendedAttributeTypes().count(), 1);

    QCOMPARE(place.extendedAttribute(QStringLiteral("smoking")).label(), QStringLiteral("Public Smoking"));
    QCOMPARE(place.extendedAttribute(QStringLiteral("smoking")).text(), QStringLiteral("No"));

    QPlaceAttribute shelter;
    shelter.setLabel(QStringLiteral("Outdoor shelter"));
    shelter.setText(QStringLiteral("Yes"));

    //test setting another new attribute
    place.setExtendedAttribute("shelter", shelter);
    QVERIFY(place.extendedAttributeTypes().contains(QStringLiteral("shelter")));
    QVERIFY(place.extendedAttributeTypes().contains(QStringLiteral("smoking")));
    QCOMPARE(place.extendedAttributeTypes().count(), 2);
    QCOMPARE(place.extendedAttribute(QStringLiteral("shelter")).label(), QStringLiteral("Outdoor shelter"));
    QCOMPARE(place.extendedAttribute(QStringLiteral("shelter")).text(), QStringLiteral("Yes"));

    //test overwriting an attribute
    shelter.setText(QStringLiteral("No"));
    place.setExtendedAttribute(QStringLiteral("shelter"), shelter);

    QVERIFY(place.extendedAttributeTypes().contains(QStringLiteral("shelter")));
    QVERIFY(place.extendedAttributeTypes().contains(QStringLiteral("smoking")));
    QCOMPARE(place.extendedAttributeTypes().count(), 2);
    QCOMPARE(place.extendedAttribute(QStringLiteral("shelter")).text(), QStringLiteral("No"));

    //test clearing of attributes by setting them to the default attribute
    foreach (const QString &attributeType, place.extendedAttributeTypes())
        place.setExtendedAttribute(attributeType, QPlaceAttribute());

    QCOMPARE(place.extendedAttributeTypes().count(), 0);

    //test removing of attributes
    place.setExtendedAttribute(QStringLiteral("smoking"), smoking);
    QVERIFY(!place.extendedAttributeTypes().isEmpty());
    place.removeExtendedAttribute(QStringLiteral("smoking"));
    QVERIFY(place.extendedAttributeTypes().isEmpty());
}
void tst_Place::visibilityTest()
{
    QPlace place;

    QCOMPARE(place.visibility(), QLocation::UnspecifiedVisibility);

    place.setVisibility(QLocation::DeviceVisibility);

    QCOMPARE(place.visibility(), QLocation::DeviceVisibility);
}

void tst_Place::isEmptyTest()
{
    QGeoLocation location;
    location.setCoordinate(QGeoCoordinate(6.788697, 51.224679));
    QVERIFY(!location.isEmpty());

    QPlaceCategory category;

    QPlaceRatings ratings;
    ratings.setCount(1);
    QVERIFY(!ratings.isEmpty());

    QPlaceSupplier supplier;
    supplier.setName(QStringLiteral("Foo & Bar Imports"));
    QVERIFY(!supplier.isEmpty());

    QPlaceIcon icon;
    QVariantMap iconParametersMap;
    iconParametersMap.insert(QStringLiteral("Para"), QStringLiteral("meter"));
    icon.setParameters(iconParametersMap);
    QVERIFY(!icon.isEmpty());

    QPlaceContent content;
    QPlaceContent::Collection contentCollection;
    contentCollection.insert(42, content);

    QPlaceAttribute attribute;
    attribute.setLabel(QStringLiteral("noodle"));

    QPlaceContactDetail contactDetail;


    QPlace place;

    // default constructed
    QVERIFY(place.isEmpty());

    // categories
    place.setCategory(category);
    QVERIFY(!place.isEmpty());
    place.categories().clear();
    place = QPlace();

    // location
    place.setLocation(location);
    QVERIFY(!place.isEmpty());
    place.setLocation(QGeoLocation());
    QVERIFY(place.isEmpty());

    // ratings
    place.setRatings(ratings);
    QVERIFY(!place.isEmpty());
    place.setRatings(QPlaceRatings());
    QVERIFY(place.isEmpty());

    // supplier
    place.setSupplier(supplier);
    QVERIFY(!place.isEmpty());
    place.setSupplier(QPlaceSupplier());
    QVERIFY(place.isEmpty());

    // attribution
    place.setAttribution(QStringLiteral("attr"));
    QVERIFY(!place.isEmpty());
    place.setAttribution(QString());
    QVERIFY(place.isEmpty());

    // icon
    place.setIcon(icon);
    QVERIFY(!place.isEmpty());
    place.setIcon(QPlaceIcon());
    QVERIFY(place.isEmpty());

    // content
    place.insertContent(QPlaceContent::EditorialType, contentCollection);
    QVERIFY(!place.isEmpty());
    place = QPlace();

    // name
    place.setName(QStringLiteral("Naniwa"));
    QVERIFY(!place.isEmpty());
    place.setName(QString());
    QVERIFY(place.isEmpty());

    // placeId
    place.setPlaceId(QStringLiteral("naniwa"));
    QVERIFY(!place.isEmpty());
    place.setPlaceId(QString());
    QVERIFY(place.isEmpty());

    // extendedAttributes
    place.setExtendedAttribute(QStringLiteral("part"), attribute);
    QVERIFY(!place.isEmpty());
    place.removeExtendedAttribute(QStringLiteral("part"));
    QVERIFY(place.isEmpty());

    // extendedAttributes
    place.setDetailsFetched(true);
    QVERIFY(place.isEmpty());

    // contact detail
    place.appendContactDetail(QStringLiteral("phone"), contactDetail);
    QVERIFY(!place.isEmpty());
    place.removeContactDetails(QStringLiteral("phone"));
    QVERIFY(place.isEmpty());

    // visibility
    place.setVisibility(QLocation::DeviceVisibility);
    QVERIFY(!place.isEmpty());
    place.setVisibility(QLocation::UnspecifiedVisibility);
    QVERIFY(place.isEmpty());
}

QTEST_APPLESS_MAIN(tst_Place)

#include "tst_qplace.moc"
