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

#include <qplaceattribute.h>

QT_USE_NAMESPACE

class tst_QPlaceAttribute : public QObject
{
    Q_OBJECT

public:
    tst_QPlaceAttribute();

private Q_SLOTS:
    void constructorTest();
    void operatorsTest();
    void isEmptyTest();
};

tst_QPlaceAttribute::tst_QPlaceAttribute()
{
}

void tst_QPlaceAttribute::constructorTest()
{
    QPlaceAttribute testObj;

    QPlaceAttribute *testObjPtr = new QPlaceAttribute(testObj);
    QVERIFY2(testObjPtr != NULL, "Copy constructor - null");
    QVERIFY2(testObjPtr->label() == QString(), "Copy constructor - wrong label");
    QVERIFY2(testObjPtr->text() == QString(), "Copy constructor - wrong text");
    QVERIFY2(*testObjPtr == testObj, "Copy constructor - compare");
    delete testObjPtr;
}

void tst_QPlaceAttribute::operatorsTest()
{
    QPlaceAttribute testObj;
    testObj.setLabel(QStringLiteral("label"));
    QPlaceAttribute testObj2;
    testObj2 = testObj;
    QVERIFY2(testObj == testObj2, "Not copied correctly");
    testObj2.setText(QStringLiteral("text"));
    QVERIFY2(testObj != testObj2, "Object should be different");
}

void tst_QPlaceAttribute::isEmptyTest()
{
    QPlaceAttribute attribute;

    QVERIFY(attribute.isEmpty());

    attribute.setLabel(QStringLiteral("label"));
    QVERIFY(!attribute.isEmpty());
    attribute.setLabel(QString());
    QVERIFY(attribute.isEmpty());

    attribute.setText(QStringLiteral("text"));
    QVERIFY(!attribute.isEmpty());
    attribute.setText(QString());
    QVERIFY(attribute.isEmpty());
}

QTEST_APPLESS_MAIN(tst_QPlaceAttribute);

#include "tst_qplaceattribute.moc"
