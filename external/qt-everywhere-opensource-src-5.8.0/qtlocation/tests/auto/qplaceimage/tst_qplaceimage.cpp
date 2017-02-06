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
#include <QtLocation/QPlaceImage>
#include <QtLocation/QPlaceUser>
#include <QtTest/QtTest>

#include "../utils/qlocationtestutils_p.h"

QT_USE_NAMESPACE

class tst_QPlaceImage : public QObject
{
    Q_OBJECT

public:
    tst_QPlaceImage();

    //needed for QLocationTestUtils::testConversion
    QPlaceImage initialSubObject();
    bool checkType(const QPlaceContent &);
    void detach(QPlaceContent *);
    void setSubClassProperty(QPlaceImage *);

private Q_SLOTS:
    void constructorTest();
    void supplierTest();
    void idTest();
    void mimeTypeTest();
    void attributionTest();
    void operatorsTest();
    void conversionTest();
};

tst_QPlaceImage::tst_QPlaceImage()
{
}

QPlaceImage tst_QPlaceImage::initialSubObject()
{
    QPlaceUser user;
    user.setName("user 1");
    user.setUserId("0001");

    QPlaceSupplier supplier;
    supplier.setName("supplier");
    supplier.setSupplierId("1");

    QPlaceImage image;
    image.setUrl(QUrl(QStringLiteral("file:///opt/icon/img.png")));
    image.setImageId("0001");
    image.setMimeType("image/png");
    image.setUser(user);
    image.setSupplier(supplier);
    image.setAttribution("attribution");

    return image;
}

bool tst_QPlaceImage::checkType(const QPlaceContent &content)
{
    return content.type() == QPlaceContent::ImageType;
}

void tst_QPlaceImage::detach(QPlaceContent *content)
{
    content->setAttribution("attribution");
}

void tst_QPlaceImage::setSubClassProperty(QPlaceImage *image)
{
    image->setImageId("0002");
}

void tst_QPlaceImage::constructorTest()
{
    QPlaceImage testObj;
    testObj.setImageId("testId");
    QPlaceImage *testObjPtr = new QPlaceImage(testObj);
    QVERIFY2(testObjPtr != NULL, "Copy constructor - null");
    QVERIFY2(*testObjPtr == testObj, "Copy constructor - compare");
    delete testObjPtr;
}

void tst_QPlaceImage::supplierTest()
{
    QPlaceImage testObj;
    QVERIFY2(testObj.supplier().supplierId() == QString(), "Wrong default value");
    QPlaceSupplier sup;
    sup.setName("testName1");
    sup.setSupplierId("testId");
    testObj.setSupplier(sup);
    QVERIFY2(testObj.supplier() == sup, "Wrong value returned");
}

void tst_QPlaceImage::idTest()
{
    QPlaceImage testObj;
    QVERIFY2(testObj.imageId() == QString(), "Wrong default value");
    testObj.setImageId("testText");
    QVERIFY2(testObj.imageId() == "testText", "Wrong value returned");
}

void tst_QPlaceImage::mimeTypeTest()
{
    QPlaceImage testObj;
    QVERIFY2(testObj.mimeType() == QString(), "Wrong default value");
    testObj.setMimeType("testText");
    QVERIFY2(testObj.mimeType() == "testText", "Wrong value returned");
}

void tst_QPlaceImage::attributionTest()
{
    QPlaceImage image;
    QVERIFY(image.attribution().isEmpty());
    image.setAttribution(QStringLiteral("Brought to you by acme"));
    QCOMPARE(image.attribution(), QStringLiteral("Brought to you by acme"));
    image.setAttribution(QString());
    QVERIFY(image.attribution().isEmpty());
}

void tst_QPlaceImage::operatorsTest()
{
    QPlaceImage testObj;
    testObj.setMimeType("testValue");
    QPlaceImage testObj2;
    testObj2 = testObj;
    QVERIFY2(testObj == testObj2, "Not copied correctly");
    testObj2.setImageId("testValue2");
    QVERIFY2(testObj != testObj2, "Object should be different");
}

void tst_QPlaceImage::conversionTest()
{
    QLocationTestUtils::testConversion<tst_QPlaceImage,
                                       QPlaceContent,
                                       QPlaceImage>(this);
}

QTEST_APPLESS_MAIN(tst_QPlaceImage);

#include "tst_qplaceimage.moc"
