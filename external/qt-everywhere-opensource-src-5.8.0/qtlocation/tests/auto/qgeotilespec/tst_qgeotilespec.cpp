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

#include "qgeotilespec_p.h"

QT_USE_NAMESPACE

class tst_QGeoTileSpec : public QObject
{
    Q_OBJECT

public:
    tst_QGeoTileSpec();

private:
    void populateGeoTileSpecData();

private Q_SLOTS:
    void constructorTest_data();
    void constructorTest();
    void pluginTest();
    void zoomTest();
    void xTest();
    void yTest();
    void mapIdTest();
    void assignsOperatorTest_data();
    void assignsOperatorTest();
    void equalsOperatorTest_data();
    void equalsOperatorTest();
    void lessThanOperatorTest_data();
    void lessThanOperatorTest();
    void qHashTest_data();
    void qHashTest();
};

tst_QGeoTileSpec::tst_QGeoTileSpec()
{
}

void tst_QGeoTileSpec::populateGeoTileSpecData(){
    QTest::addColumn<QString>("plugin");
    QTest::addColumn<int>("mapId");
    QTest::addColumn<int>("zoom");
    QTest::addColumn<int>("x");
    QTest::addColumn<int>("y");
    QTest::newRow("zeros") << QString() << 0 << 0 << 0 << 0;
    QTest::newRow("valid") << QString("geo plugin") << 455 << 1 << 20 << 50;
    QTest::newRow("negative values") << QString("geo plugin negative") << -350 << 2 << -20 << -50;
}

void tst_QGeoTileSpec::constructorTest_data()
{
    populateGeoTileSpecData();
}

void tst_QGeoTileSpec::constructorTest()
{
    QFETCH(QString,plugin);
    QFETCH(int,zoom);
    QFETCH(int,mapId);
    QFETCH(int,x);
    QFETCH(int,y);

    // test constructor copy with default values
    QGeoTileSpec testObj;
    QGeoTileSpec testObj2(testObj);
    QCOMPARE(testObj.plugin(), testObj2.plugin());
    QCOMPARE(testObj.mapId(), testObj2.mapId());
    QCOMPARE(testObj.zoom(), testObj2.zoom());
    QCOMPARE(testObj.x(), testObj2.x());
    QCOMPARE(testObj.y(), testObj2.y());

    // test second construct
    QGeoTileSpec testObj3(plugin, mapId, zoom, x, y);
    QCOMPARE(testObj3.plugin(), plugin);
    QCOMPARE(testObj3.mapId(), mapId);
    QCOMPARE(testObj3.zoom(), zoom);
    QCOMPARE(testObj3.x(), x);
    QCOMPARE(testObj3.y(), y);
}

void tst_QGeoTileSpec::pluginTest()
{
    QGeoTileSpec tileSpec;
    QCOMPARE(tileSpec.plugin(), QString());

    QGeoTileSpec tileSpec2(QString("plugin test"),1,10,10,5);
    QCOMPARE(tileSpec2.plugin(), QString("plugin test"));
}

void tst_QGeoTileSpec::zoomTest()
{
    QGeoTileSpec tileSpec;
    QVERIFY(tileSpec.zoom() == -1);
    tileSpec.setZoom(1);
    QVERIFY(tileSpec.zoom() == 1);

    QGeoTileSpec tileSpec2 = tileSpec;
    QVERIFY(tileSpec2.zoom() == 1);
    tileSpec.setZoom(2);
    QVERIFY(tileSpec2.zoom() == 1);
}

void tst_QGeoTileSpec::xTest()
{
    QGeoTileSpec tileSpec;
    QVERIFY(tileSpec.x() == -1);
    tileSpec.setX(10);
    QVERIFY(tileSpec.x() == 10);

    QGeoTileSpec tileSpec2 = tileSpec;
    QVERIFY(tileSpec2.x() == 10);
    tileSpec.setX(30);
    QVERIFY(tileSpec2.x() == 10);
}

void tst_QGeoTileSpec::yTest()
{
    QGeoTileSpec tileSpec;
    QVERIFY(tileSpec.y() == -1);
    tileSpec.setY(20);
    QVERIFY(tileSpec.y() == 20);

    QGeoTileSpec tileSpec2 = tileSpec;
    QVERIFY(tileSpec2.y() == 20);
    tileSpec.setY(40);
    QVERIFY(tileSpec2.y() == 20);
}

void tst_QGeoTileSpec::mapIdTest()
{
    QGeoTileSpec tileSpec;
    QVERIFY(tileSpec.mapId() == 0);
    tileSpec.setMapId(1);
    QVERIFY(tileSpec.mapId() == 1);

    QGeoTileSpec tileSpec2 = tileSpec;
    QVERIFY(tileSpec2.mapId() == 1);
    tileSpec.setMapId(5);
    QVERIFY(tileSpec2.mapId() == 1);
}

void tst_QGeoTileSpec::assignsOperatorTest_data()
{
    populateGeoTileSpecData();
}


void tst_QGeoTileSpec::assignsOperatorTest()
{
    QFETCH(QString,plugin);
    QFETCH(int,mapId);
    QFETCH(int,zoom);
    QFETCH(int,x);
    QFETCH(int,y);

    QGeoTileSpec testObj(plugin, mapId, zoom, x, y);
    QGeoTileSpec testObj2;
    testObj2 = testObj;
    // test the correctness of the asignment operator
    QVERIFY2(testObj2.plugin() == plugin, "Plugin not copied correctly");
    QVERIFY2(testObj2.zoom() == zoom, "Zoom not copied correctly");
    QVERIFY2(testObj2.mapId() == mapId, "Map Id not copied correctly");
    QVERIFY2(testObj2.x() == x, "X not copied correctly");
    QVERIFY2(testObj2.y() == y, "Y not copied correctly");
    // verify that values have not changed after an assignment
    QVERIFY2(testObj.plugin() == testObj2.plugin(), "Plugin not copied correctly");
    QVERIFY2(testObj.zoom() == testObj2.zoom(), "Zoom not copied correctly");
    QVERIFY2(testObj.mapId() == testObj2.mapId(), "Map Id not copied correctly");
    QVERIFY2(testObj.x() == testObj2.x(), "X not copied correctly");
    QVERIFY2(testObj.y() == testObj2.y(), "Y not copied correctly");
}


void tst_QGeoTileSpec::equalsOperatorTest_data()
{
    populateGeoTileSpecData();
}

void tst_QGeoTileSpec::equalsOperatorTest()
{
    QFETCH(QString,plugin);
    QFETCH(int,mapId);
    QFETCH(int,zoom);
    QFETCH(int,x);
    QFETCH(int,y);

    QGeoTileSpec testObj(plugin, mapId, zoom, x, y);
    QGeoTileSpec testObj2(plugin, mapId, zoom, x, y);
    QVERIFY2(testObj == testObj2, "Equals operator is not correct");

    // test QGeoTileSpec pairs where they differ in one field
    testObj2.setZoom(zoom+1);
    QVERIFY2(!(testObj == testObj2), "Equals operator is not correct");
    testObj2 = testObj;
    testObj2.setMapId(mapId+1);
    QVERIFY2(!(testObj == testObj2), "Equals operator is not correct");
    testObj2 = testObj;
    testObj2.setX(x+1);
    QVERIFY2(!(testObj == testObj2), "Equals operator is not correct");
    testObj2 = testObj;
    testObj2.setY(y+1);
    QVERIFY2(!(testObj == testObj2), "Equals operator is not correct");
}

void tst_QGeoTileSpec::lessThanOperatorTest_data()
{
    populateGeoTileSpecData();
}

void tst_QGeoTileSpec::lessThanOperatorTest()
{
    QFETCH(QString,plugin);
    QFETCH(int,mapId);
    QFETCH(int,zoom);
    QFETCH(int,x);
    QFETCH(int,y);

    QGeoTileSpec testObj(plugin, mapId, zoom, x, y);
    QGeoTileSpec testObj2(testObj);
    QVERIFY(!(testObj < testObj2));

    testObj2.setMapId(mapId-1);
    QVERIFY2(testObj2 < testObj, "Less than operator is not correct for mapId");
    testObj2 = testObj;
    testObj2.setZoom(zoom-1);
    QVERIFY2(testObj2 < testObj, "Less than operator is not correct for zoom");
    testObj2 = testObj;
    testObj2.setX(x-1);
    QVERIFY2(testObj2 < testObj, "Less than operator is not correct for x");
    testObj2 = testObj;
    testObj2.setY(y-1);
    QVERIFY2(testObj2 < testObj, "Less than operator is not correct for y");

    // less than comparisons are done in the order: plugin -> mapId -> zoom -> x -> y
    // the test below checks if the order is correct
    QGeoTileSpec testObj3(plugin + QString('a'), mapId-1, zoom-1, x-1, y-1);
    QVERIFY2(testObj < testObj3, "Order of less than operator is not correct");
    QGeoTileSpec testObj4(plugin, mapId+1, zoom-1, x-1, y-1);
    QVERIFY2(testObj < testObj4, "Order of less than operator is not correct");
    QGeoTileSpec testObj5(plugin, mapId, zoom+1, x-1, y-1);
    QVERIFY2(testObj < testObj5, "Order of less than operator is not correct");
    QGeoTileSpec testObj6(plugin, mapId, zoom, x+1, y-1);
    QVERIFY2(testObj < testObj6, "Order of less than operator is not correct");
    QGeoTileSpec testObj7(plugin, mapId, zoom, x, y+1);
    QVERIFY2(testObj < testObj7, "Order of less than operator is not correct");

    QGeoTileSpec testObj8(plugin, mapId-1, zoom+1, x+1, y+1);
    QVERIFY2(testObj8 < testObj, "Order of less than operator is not correct");
    QGeoTileSpec testObj9(plugin, mapId, zoom-1, x+1, y+1);
    QVERIFY2(testObj9 < testObj, "Order of less than operator is not correct");
    QGeoTileSpec testObj10(plugin, mapId, zoom, x-1, y+1);
    QVERIFY2(testObj10 < testObj, "Order of less than operator is not correct");
}

void tst_QGeoTileSpec::qHashTest_data(){
    populateGeoTileSpecData();
}

void tst_QGeoTileSpec::qHashTest()
{
    QGeoTileSpec testObj;
    unsigned int hash1 = qHash(testObj);
    QGeoTileSpec testObj2;
    testObj2 = testObj;
    unsigned int hash2 = qHash(testObj2);
    QCOMPARE(hash1, hash2);

    QFETCH(QString,plugin);
    QFETCH(int,mapId);
    QFETCH(int,zoom);
    QFETCH(int,x);
    QFETCH(int,y);

    QGeoTileSpec testObj3(plugin, mapId, zoom, x, y);
    unsigned int hash3 = qHash(testObj3);
    QVERIFY(hash1 != hash3);

    testObj2.setMapId(testObj3.mapId()+1);
    testObj2.setZoom(testObj3.zoom()+1);
    testObj2.setX(testObj3.x()*5);
    testObj2.setY(testObj3.y()*10);
    hash2 = qHash(testObj2);
    QVERIFY(hash2 != hash3);
}

QTEST_APPLESS_MAIN(tst_QGeoTileSpec)

#include "tst_qgeotilespec.moc"


