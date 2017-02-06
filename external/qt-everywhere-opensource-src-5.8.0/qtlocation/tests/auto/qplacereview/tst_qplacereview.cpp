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

#include <QtLocation/QPlaceReview>
#include <QtLocation/QPlaceSupplier>
#include <QtLocation/QPlaceUser>

#include "../utils/qlocationtestutils_p.h"

QT_USE_NAMESPACE

class tst_QPlaceReview : public QObject
{
    Q_OBJECT

public:
    tst_QPlaceReview();

    //needed for QLocationTestUtils::testConversion
    QPlaceReview initialSubObject();
    bool checkType(const QPlaceContent &);
    void detach(QPlaceContent *);
    void setSubClassProperty(QPlaceReview *);

private Q_SLOTS:
    void constructorTest();
    void supplierTest();
    void dateTest();
    void textTest();
    void languageTest();
    void ratingTest();
    void reviewIdTest();
    void titleTest();
    void userTest();
    void operatorsTest();
    void conversionTest();
};

tst_QPlaceReview::tst_QPlaceReview()
{
}

QPlaceReview tst_QPlaceReview::initialSubObject()
{
    QPlaceUser user;
    user.setName("user 1");
    user.setUserId("0001");

    QPlaceSupplier supplier;
    supplier.setName("supplier");
    supplier.setSupplierId("1");

    QPlaceReview review;
    review.setTitle("title");
    review.setText("text");
    review.setRating(4.5);
    review.setLanguage("en");
    review.setDateTime(QDateTime::fromString("01:02 03/04/2000",
                                             "hh:mm dd/MM/yyyy"));
    review.setUser(user);
    review.setSupplier(supplier);
    review.setAttribution("attribution");

    return review;
}

bool tst_QPlaceReview::checkType(const QPlaceContent &content)
{
    return content.type() == QPlaceContent::ReviewType;
}

void tst_QPlaceReview::detach(QPlaceContent *content)
{
    content->setAttribution("attribution");
}

void tst_QPlaceReview::setSubClassProperty(QPlaceReview *review)
{
    review->setTitle("new title");
}

void tst_QPlaceReview::constructorTest()
{
    QPlaceReview testObj;
    testObj.setLanguage("testId");
    QPlaceReview *testObjPtr = new QPlaceReview(testObj);
    QVERIFY2(testObjPtr != NULL, "Copy constructor - null");
    QVERIFY2(*testObjPtr == testObj, "Copy constructor - compare");
    delete testObjPtr;
}

void tst_QPlaceReview::supplierTest()
{
    QPlaceReview testObj;
    QVERIFY2(testObj.supplier().supplierId() == QString(), "Wrong default value");
    QPlaceSupplier sup;
    sup.setName("testName1");
    sup.setSupplierId("testId");
    testObj.setSupplier(sup);
    QVERIFY2(testObj.supplier() == sup, "Wrong value returned");
}

void tst_QPlaceReview::dateTest()
{
    QPlaceReview testObj;
    QCOMPARE(testObj.dateTime(), QDateTime());

    QDateTime dt = QDateTime::currentDateTime();
    testObj.setDateTime(dt);
    QCOMPARE(testObj.dateTime(), dt);
}

void tst_QPlaceReview::textTest()
{
    QPlaceReview testObj;
    QVERIFY2(testObj.text() == QString(), "Wrong default value");
    testObj.setText("testText");
    QVERIFY2(testObj.text() == "testText", "Wrong value returned");
}

void tst_QPlaceReview::languageTest()
{
    QPlaceReview testObj;
    QVERIFY2(testObj.language() == QString(), "Wrong default value");
    testObj.setLanguage("testText");
    QVERIFY2(testObj.language() == "testText", "Wrong value returned");
}

void tst_QPlaceReview::ratingTest()
{
    QPlaceReview testObj;
    QVERIFY2(testObj.rating() == 0, "Wrong default value");
    testObj.setRating(-10);
    QCOMPARE(testObj.rating(), -10.0);
    testObj.setRating(3.4);
    QCOMPARE(testObj.rating(), 3.4);
}

void tst_QPlaceReview::operatorsTest()
{
    QPlaceReview testObj;
    testObj.setText("testValue");
    QPlaceReview testObj2;
    testObj2 = testObj;
    QVERIFY2(testObj == testObj2, "Not copied correctly");
    testObj2.setLanguage("testValue2");
    QVERIFY2(testObj != testObj2, "Object should be different");
}

void tst_QPlaceReview::reviewIdTest()
{
    QPlaceReview testObj;
    QVERIFY2(testObj.reviewId() == QString(), "Wrong default value");
    testObj.setReviewId("testText");
    QVERIFY2(testObj.reviewId() == "testText", "Wrong value returned");
}
void tst_QPlaceReview::titleTest()
{
    QPlaceReview testObj;
    QVERIFY2(testObj.title() == QString(), "Wrong default value");
    testObj.setTitle("testText");
    QVERIFY2(testObj.title() == "testText", "Wrong value returned");
}

void tst_QPlaceReview::userTest()
{
    QPlaceReview review;
    QVERIFY(review.user().userId().isEmpty());
    QVERIFY(review.user().name().isEmpty());
    QPlaceUser user;
    user.setUserId(QStringLiteral("11111"));
    user.setName(QStringLiteral("Bob"));

    review.setUser(user);
    QCOMPARE(review.user().userId(), QStringLiteral("11111"));
    QCOMPARE(review.user().name(), QStringLiteral("Bob"));

    review.setUser(QPlaceUser());
    QVERIFY(review.user().userId().isEmpty());
    QVERIFY(review.user().name().isEmpty());
}

void tst_QPlaceReview::conversionTest()
{
    QLocationTestUtils::testConversion<tst_QPlaceReview,
                                       QPlaceContent,
                                       QPlaceReview>(this);
}

QTEST_APPLESS_MAIN(tst_QPlaceReview)

#include "tst_qplacereview.moc"
