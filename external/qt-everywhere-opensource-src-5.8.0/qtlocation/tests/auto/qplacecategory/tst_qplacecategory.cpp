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

#include <qplacecategory.h>
#include <qplaceicon.h>

QT_USE_NAMESPACE

class tst_QPlaceCategory : public QObject
{
    Q_OBJECT

public:
    tst_QPlaceCategory();

private Q_SLOTS:
    void constructorTest();
    void categoryIdTest();
    void nameTest();
    void visibilityTest();
    void operatorsTest();
    void isEmptyTest();
};

tst_QPlaceCategory::tst_QPlaceCategory()
{
}

void tst_QPlaceCategory::constructorTest()
{
    QPlaceCategory testObj;
    Q_UNUSED(testObj);

    testObj.setCategoryId("testId");
    QPlaceCategory *testObjPtr = new QPlaceCategory(testObj);
    QVERIFY2(testObjPtr != NULL, "Copy constructor - null");
    QVERIFY2(*testObjPtr == testObj, "Copy constructor - compare");
    delete testObjPtr;
}

void tst_QPlaceCategory::categoryIdTest()
{
    QPlaceCategory testObj;
    QVERIFY2(testObj.categoryId() == QString(), "Wrong default value");
    testObj.setCategoryId("testText");
    QVERIFY2(testObj.categoryId() == "testText", "Wrong value returned");
}

void tst_QPlaceCategory::nameTest()
{
    QPlaceCategory testObj;
    QVERIFY2(testObj.name() == QString(), "Wrong default value");
    testObj.setName("testText");
    QVERIFY2(testObj.name() == "testText", "Wrong value returned");
}

void tst_QPlaceCategory::visibilityTest()
{
    QPlaceCategory category;

    QCOMPARE(category.visibility(), QLocation::UnspecifiedVisibility);

    category.setVisibility(QLocation::DeviceVisibility);

    QCOMPARE(category.visibility(), QLocation::DeviceVisibility);
}

void tst_QPlaceCategory::operatorsTest()
{
    QPlaceCategory testObj;
    testObj.setName("testValue");
    QPlaceCategory testObj2;
    testObj2 = testObj;
    QVERIFY2(testObj == testObj2, "Not copied correctly");
    testObj2.setCategoryId("a3rfg");
    QVERIFY2(testObj != testObj2, "Object should be different");
}

void tst_QPlaceCategory::isEmptyTest()
{
    QPlaceIcon icon;
    QVariantMap parameters;
    parameters.insert(QStringLiteral("para"), QStringLiteral("meter"));
    icon.setParameters(parameters);
    QVERIFY(!icon.isEmpty());

    QPlaceCategory category;

    QVERIFY(category.isEmpty());

    category.setName(QStringLiteral("name"));
    QVERIFY(!category.isEmpty());
    category.setName(QString());
    QVERIFY(category.isEmpty());

    category.setCategoryId(QStringLiteral("id"));
    QVERIFY(!category.isEmpty());
    category.setCategoryId(QString());
    QVERIFY(category.isEmpty());

    category.setVisibility(QLocation::PublicVisibility);
    QVERIFY(!category.isEmpty());
    category.setVisibility(QLocation::UnspecifiedVisibility);
    QVERIFY(category.isEmpty());

    category.setIcon(icon);
    QVERIFY(!category.isEmpty());
    category.setIcon(QPlaceIcon());
    QVERIFY(category.isEmpty());
}

QTEST_APPLESS_MAIN(tst_QPlaceCategory);

#include "tst_qplacecategory.moc"
