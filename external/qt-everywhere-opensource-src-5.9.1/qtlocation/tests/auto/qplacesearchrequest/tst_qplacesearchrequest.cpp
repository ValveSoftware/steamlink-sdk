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
#include <QtLocation/QPlaceSearchRequest>
#include <QtPositioning/QGeoCircle>
#include <QtPositioning/QGeoRectangle>

QT_USE_NAMESPACE

class tst_QPlaceSearchRequest : public QObject
{
    Q_OBJECT

public:
    tst_QPlaceSearchRequest();

private Q_SLOTS:
    void constructorTest();
    void searchTermTest();
    void categoriesTest();
    void boundingCircleTest();
    void boundingBoxTest();
    void searchAreaTest();
    void visibilityScopeTest();
    void relevanceHintTest();
    void searchContextTest();
    void operatorsTest();
    void clearTest();
};

tst_QPlaceSearchRequest::tst_QPlaceSearchRequest()
{
}

void tst_QPlaceSearchRequest::constructorTest()
{
    QPlaceSearchRequest testObj;
    Q_UNUSED(testObj);

    QPlaceSearchRequest *testObjPtr = new QPlaceSearchRequest(testObj);
    QVERIFY2(testObjPtr != NULL, "Copy constructor - null");
    QVERIFY2(*testObjPtr == testObj, "Copy constructor - compare");
    delete testObjPtr;
}

void tst_QPlaceSearchRequest::searchTermTest()
{
    QPlaceSearchRequest testObj;
    QVERIFY2(testObj.searchTerm() == QString(), "Wrong default value");
    testObj.setSearchTerm("testText");
    QVERIFY2(testObj.searchTerm() == "testText", "Wrong value returned");
}

void tst_QPlaceSearchRequest::categoriesTest()
{
    QPlaceSearchRequest testObj;
    QVERIFY2(testObj.categories().count() == 0, "Wrong default value");
    QPlaceCategory cat;
    cat.setCategoryId("45346");
    testObj.setCategory(cat);
    QVERIFY2(testObj.categories().count() == 1, "Wrong categories count returned");
    QVERIFY2(testObj.categories()[0] == cat, "Wrong category returned");

    testObj.setCategory(QPlaceCategory());
    QVERIFY(testObj.categories().isEmpty());
}

void tst_QPlaceSearchRequest::boundingCircleTest()
{
    QPlaceSearchRequest query;
    QVERIFY2(query.searchArea() == QGeoShape(), "Wrong default value");
    QGeoCircle circle;
    circle.setCenter(QGeoCoordinate(30,20));
    circle.setRadius(500.0);
    query.setSearchArea(circle);

    QVERIFY(query.searchArea() != QGeoShape());
    QVERIFY(query.searchArea().type() == QGeoShape::CircleType);
    QVERIFY(query.searchArea() == circle);

    QGeoCircle retrievedCircle = query.searchArea();
    QVERIFY2(retrievedCircle.center() == QGeoCoordinate(30,20), "Wrong value returned");
    QVERIFY2(retrievedCircle.radius() == 500.0, "Wrong value returned");
    query.clear();
    QVERIFY2(query.searchArea() == QGeoShape(), "Search area not cleared");
}

void tst_QPlaceSearchRequest::boundingBoxTest()
{
    QPlaceSearchRequest query;
    QVERIFY2(query.searchArea() == QGeoShape(), "Wrong default value");
    QGeoRectangle box;

    box.setTopLeft(QGeoCoordinate(30,20));
    box.setBottomRight(QGeoCoordinate(10,50));
    query.setSearchArea(box);

    QVERIFY(query.searchArea() != QGeoShape());
    QVERIFY(query.searchArea().type() == QGeoShape::RectangleType);
    QVERIFY(query.searchArea() == box);

    QGeoRectangle retrievedBox = query.searchArea();
    QVERIFY2(retrievedBox.topLeft() == QGeoCoordinate(30,20), "Wrong value returned");
    QVERIFY2(retrievedBox.bottomRight() == QGeoCoordinate(10,50), "Wrong value returned");

    query.clear();
    QVERIFY2(query.searchArea() == QGeoShape(), "Wrong cleared value returned");
}

void tst_QPlaceSearchRequest::searchAreaTest()
{
    //test assignment of new search area over an old search area
    QPlaceSearchRequest *query = new QPlaceSearchRequest;
    QGeoCircle circle;
    circle.setCenter(QGeoCoordinate(30,20));
    circle.setRadius(500.0);
    query->setSearchArea(circle);

    QVERIFY(query->searchArea() == circle);
    QGeoRectangle box;
    box.setTopLeft(QGeoCoordinate(30,20));
    box.setBottomRight(QGeoCoordinate(10,50));
    query->setSearchArea(box);
    QVERIFY2(query->searchArea() == box, "New search area not assigned");
}

void tst_QPlaceSearchRequest::visibilityScopeTest()
{
    QPlaceSearchRequest query;
    QVERIFY2(query.visibilityScope() == QLocation::UnspecifiedVisibility, "Wrong default value");

    query.setVisibilityScope(QLocation::DeviceVisibility);
    QCOMPARE(query.visibilityScope(), QLocation::DeviceVisibility);

    query.setVisibilityScope(QLocation::DeviceVisibility | QLocation::PublicVisibility);
    QVERIFY(query.visibilityScope() & QLocation::DeviceVisibility);
    QVERIFY(!(query.visibilityScope() & QLocation::PrivateVisibility));
    QVERIFY(query.visibilityScope() & QLocation::PublicVisibility);
}

void tst_QPlaceSearchRequest::relevanceHintTest()
{
    QPlaceSearchRequest request;
    QCOMPARE(request.relevanceHint(), QPlaceSearchRequest::UnspecifiedHint);
    request.setRelevanceHint(QPlaceSearchRequest::DistanceHint);
    QCOMPARE(request.relevanceHint(), QPlaceSearchRequest::DistanceHint);
    request.setRelevanceHint(QPlaceSearchRequest::UnspecifiedHint);
    QCOMPARE(request.relevanceHint(), QPlaceSearchRequest::UnspecifiedHint);
}

void tst_QPlaceSearchRequest::searchContextTest()
{
    QPlaceSearchRequest request;
    QVERIFY(!request.searchContext().value<QUrl>().isValid());
    request.setSearchContext(QUrl(QStringLiteral("http://www.example.com/")));
    QCOMPARE(request.searchContext().value<QUrl>(), QUrl(QStringLiteral("http://www.example.com/")));
}

void tst_QPlaceSearchRequest::operatorsTest()
{
    QPlaceSearchRequest testObj;
    testObj.setSearchTerm(QStringLiteral("testValue"));
    QPlaceSearchRequest testObj2;
    testObj2 = testObj;
    QVERIFY2(testObj == testObj2, "Not copied correctly");
    testObj2.setSearchTerm(QStringLiteral("abc"));
    QVERIFY2(testObj != testObj2, "Object should be different");
    testObj2.setSearchTerm(QStringLiteral("testValue"));
    QVERIFY(testObj == testObj2);

    QGeoRectangle b1(QGeoCoordinate(20,20), QGeoCoordinate(10,30));
    QGeoRectangle b2(QGeoCoordinate(20,20), QGeoCoordinate(10,30));
    QGeoRectangle b3(QGeoCoordinate(40,40), QGeoCoordinate(10,40));

    //testing that identical boxes match
    testObj.setSearchArea(b1);
    testObj2.setSearchArea(b2);
    QVERIFY2(testObj == testObj2, "Identical box areas are not identified as matching");

    //test that different boxes do not match
    testObj2.setSearchArea(b3);
    QVERIFY2(testObj != testObj2, "Different box areas identified as matching");

    QGeoCircle c1(QGeoCoordinate(5,5),500);
    QGeoCircle c2(QGeoCoordinate(5,5),500);
    QGeoCircle c3(QGeoCoordinate(9,9),600);

    //test that identical cirlces match
    testObj.setSearchArea(c1);
    testObj2.setSearchArea(c2);
    QVERIFY2(testObj == testObj2, "Identical circle areas are not identified as matching");

    //test that different circle don't match
    testObj2.setSearchArea(c3);
    QVERIFY2(testObj != testObj2, "Different circle areas identified as matching");

    //test that circles and boxes do not match
    QGeoRectangle b4(QGeoCoordinate(20,20),QGeoCoordinate(10,30));
    QGeoCircle c4(QGeoCoordinate(20,20),500);
    testObj.setSearchArea(b4);
    testObj2.setSearchArea(c4);
    QVERIFY2(testObj != testObj2, "Circle and box identified as matching");

    //test that identical visibility scopes match
    testObj.clear();
    testObj2.clear();
    testObj.setVisibilityScope(QLocation::PublicVisibility);
    testObj2.setVisibilityScope(QLocation::PublicVisibility);
    QVERIFY2(testObj == testObj2, "Identical scopes not identified as matching");

    //test that different scopes do not match
    testObj2.setVisibilityScope(QLocation::PrivateVisibility);
    QVERIFY2(testObj != testObj2, "Different scopes identified as matching");

    //test that different search contexts do not match
    testObj.clear();
    testObj2.clear();
    testObj2.setSearchContext(QUrl(QStringLiteral("http://www.example.com/")));
    QVERIFY(testObj != testObj2);
}

void tst_QPlaceSearchRequest::clearTest()
{
    QPlaceSearchRequest req;
    req.setSearchTerm("pizza");
    req.setSearchArea(QGeoCircle(QGeoCoordinate(1,1), 5000));
    QPlaceCategory category;
    category.setName("Fast Food");
    req.setCategory(category);
    req.setLimit(100);

    req.clear();
    QVERIFY(req.searchTerm().isEmpty());
    QVERIFY(req.searchArea() == QGeoShape());
    QVERIFY(req.categories().isEmpty());
    QVERIFY(req.limit() == -1);
}

QTEST_APPLESS_MAIN(tst_QPlaceSearchRequest)

#include "tst_qplacesearchrequest.moc"
