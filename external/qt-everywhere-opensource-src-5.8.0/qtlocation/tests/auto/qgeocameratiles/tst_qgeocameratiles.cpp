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

//TESTED_COMPONENT=src/location/maps

#include "qgeotilespec_p.h"
#include "qgeocameratiles_p.h"
#include "qgeocameradata_p.h"
#include "qgeomaptype_p.h"

#include <QtPositioning/private/qgeoprojection_p.h>
#include <QtPositioning/private/qdoublevector2d_p.h>
#include <QtTest/QtTest>
#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/qmath.h>

QT_USE_NAMESPACE

struct PositionTestInfo {
    QString xyString;
    QString zoomString;
    QString wString;
    QString hString;
    double x;
    double y;
    double zoom;
    double w;
    double h;
};

class tst_QGeoCameraTiles : public QObject
{
    Q_OBJECT

private:

    void row(const PositionTestInfo &pti, int xOffset, int yOffset, int tileX, int tileY, int tileW, int tileH);
    void test_group(const PositionTestInfo &pti, QList<int> &xVals, QList<int> &wVals, QList<int> &yVals, QList<int> &hVals);

private slots:

    void tilesPlugin();
    void tilesMapType();
    void tilesPositions();
    void tilesPositions_data();
};

void tst_QGeoCameraTiles::row(const PositionTestInfo &pti, int xOffset, int yOffset, int tileX, int tileY, int tileW, int tileH)
{
    double step = 1 / (qPow(2.0, 4.0) * 4);

    QString row = pti.xyString;
    row += QStringLiteral(" - ");
    row += pti.zoomString;
    row += QStringLiteral(" - (");
    row += QString::number(xOffset);
    row += QStringLiteral(",");
    row += QString::number(yOffset);
    row += QStringLiteral(") - ");
    row += pti.wString;
    row += QStringLiteral(" x ");
    row += pti.hString;

    QList<int> xRow;
    QList<int> yRow;

    for (int y = 0; y < tileH; ++y) {
        for (int x = 0; x < tileW; ++x) {
            if (tileX + x < 16)
                xRow << tileX + x;
            else
                xRow << tileX + x - 16;
            yRow << tileY + y;
        }
    }

    QTest::newRow(qPrintable(row))
            << pti.x + step * xOffset << pti.y + step * yOffset
            << pti.zoom << pti.w << pti.h
            << xRow
            << yRow;
}

void tst_QGeoCameraTiles::test_group(const PositionTestInfo &pti, QList<int> &xVals, QList<int> &wVals, QList<int> &yVals, QList<int> &hVals)
{
    for (int x = 0; x < 5; ++x) {
        for (int y = 0; y < 5; ++y) {
            row(pti, x, y, xVals.at(x), yVals.at(y), wVals.at(x), hVals.at(y));
        }
    }
}

void tst_QGeoCameraTiles::tilesPlugin()
{
    QGeoCameraData camera;
    camera.setZoomLevel(4.0);
    camera.setCenter(QGeoCoordinate(0.0, 0.0));

    QGeoCameraTiles ct;
    ct.setTileSize(16);
    ct.setCameraData(camera);
    ct.setScreenSize(QSize(32, 32));
    ct.setMapType(QGeoMapType(QGeoMapType::StreetMap, "street map", "street map", false, false, 1));

    QSet<QGeoTileSpec> tiles1 = ct.createTiles();

    ct.setPluginString("pluginA");

    QSet<QGeoTileSpec> tiles2 = ct.createTiles();

    typedef QSet<QGeoTileSpec>::const_iterator iter;
    iter i1 = tiles1.constBegin();
    iter end1 = tiles1.constEnd();

    QSet<QGeoTileSpec> tiles2_check;

    for (; i1 != end1; ++i1) {
        QGeoTileSpec tile = *i1;
        tiles2_check.insert(QGeoTileSpec("pluginA", tile.mapId(), tile.zoom(), tile.x(), tile.y()));
    }

    QCOMPARE(tiles2, tiles2_check);

    ct.setPluginString("pluginB");

    QSet<QGeoTileSpec> tiles3 = ct.createTiles();

    iter i2 = tiles2.constBegin();
    iter end2 = tiles2.constEnd();

    QSet<QGeoTileSpec> tiles3_check;

    for (; i2 != end2; ++i2) {
        QGeoTileSpec tile = *i2;
        tiles3_check.insert(QGeoTileSpec("pluginB", tile.mapId(), tile.zoom(), tile.x(), tile.y()));
    }

    QCOMPARE(tiles3, tiles3_check);
}

void tst_QGeoCameraTiles::tilesMapType()
{
    QGeoCameraData camera;
    camera.setZoomLevel(4.0);
    camera.setCenter(QGeoCoordinate(0.0, 0.0));

    QGeoCameraTiles ct;
    ct.setTileSize(16);
    ct.setCameraData(camera);
    ct.setScreenSize(QSize(32, 32));
    ct.setPluginString("pluginA");

    QSet<QGeoTileSpec> tiles1 = ct.createTiles();

    QGeoMapType mapType1 = QGeoMapType(QGeoMapType::StreetMap, "street map", "street map", false, false, 1);
    ct.setMapType(mapType1);

    QSet<QGeoTileSpec> tiles2 = ct.createTiles();

    typedef QSet<QGeoTileSpec>::const_iterator iter;
    iter i1 = tiles1.constBegin();
    iter end1 = tiles1.constEnd();

    QSet<QGeoTileSpec> tiles2_check;

    for (; i1 != end1; ++i1) {
        QGeoTileSpec tile = *i1;
        tiles2_check.insert(QGeoTileSpec(tile.plugin(), mapType1.mapId(), tile.zoom(), tile.x(), tile.y()));
    }

    QCOMPARE(tiles2, tiles2_check);

    QGeoMapType mapType2 = QGeoMapType(QGeoMapType::StreetMap, "satellite map", "satellite map", false, false, 2);
    ct.setMapType(mapType2);

    QSet<QGeoTileSpec> tiles3 = ct.createTiles();

    iter i2 = tiles2.constBegin();
    iter end2 = tiles2.constEnd();

    QSet<QGeoTileSpec> tiles3_check;

    for (; i2 != end2; ++i2) {
        QGeoTileSpec tile = *i2;
        tiles3_check.insert(QGeoTileSpec(tile.plugin(), mapType2.mapId(), tile.zoom(), tile.x(), tile.y()));
    }

    QCOMPARE(tiles3, tiles3_check);
}

void tst_QGeoCameraTiles::tilesPositions()
{
    QFETCH(double, mercatorX);
    QFETCH(double, mercatorY);
    QFETCH(double, zoom);
    QFETCH(double, width);
    QFETCH(double, height);
    QFETCH(QList<int> , tilesX);
    QFETCH(QList<int> , tilesY);

    QGeoCameraData camera;
    camera.setZoomLevel(zoom);
    camera.setCenter(QGeoProjection::mercatorToCoord(QDoubleVector2D(mercatorX, mercatorY)));

    QGeoCameraTiles ct;
    ct.setTileSize(16);
    ct.setCameraData(camera);
    ct.setScreenSize(QSize(qCeil(width), qCeil(height)));

    QSet<QGeoTileSpec> tiles;

    QVERIFY2(tilesX.size() == tilesY.size(), "tilesX and tilesY have different size");

    for (int i = 0; i < tilesX.size(); ++i)
        tiles.insert(QGeoTileSpec("", 0, static_cast<int>(qFloor(zoom)), tilesX.at(i), tilesY.at(i)));

    QCOMPARE(ct.createTiles(), tiles);
}

void tst_QGeoCameraTiles::tilesPositions_data()
{
    QTest::addColumn<double>("mercatorX");
    QTest::addColumn<double>("mercatorY");
    QTest::addColumn<double>("zoom");
    QTest::addColumn<double>("width");
    QTest::addColumn<double>("height");
    QTest::addColumn<QList<int> >("tilesX");
    QTest::addColumn<QList<int> >("tilesY");

    int t = 16;

    PositionTestInfo pti;

    /*
            This test sets up various viewports onto a 16x16 map,
            and checks which tiles are visible against those that
            are expected to be visible.

            The tests are run in 5 groups, corresponding to where
            the viewport is centered on the map.

            The groups are named as follows, with the tile in
            which the viewport is centered listed in parenthesis:
              - mid (8, 8)
              - top (8, 0)
              - bottom (8, 15)
              - left (0, 8)
              - right (15, 8)

            For each of these groups a number of tests are run,
            which involve modifying various parameters.

            If "t" is the width of a tile, the width and height
            of the viewport take on values including:
              - (t - 1)
              - t
              - (t + 1)
              - (2t - 1)
              - 2t
              - (2t + 1)

            The viewport is also offset by fractions of a tile
            in both the x and y directions.  The offsets are in
            quarters of a tile.

            The diagrams below present a justification for our
            test expectations.

            The diagrams show variations in viewport width and
            x offset into the target tile , although can easily
            be taken to be the variations in viewport height and
            y offset into the target tile.

            The symbols have the following meanings:
              "+" - tile boundaries
              "*" - viewport boundary
              "T" - boundary of tile the viewport is centered on
              "O" - same as "T" but coincident with the viewport boundary

            Whenever the viewport boundary is coincident with a tile boundary,
            the tiles on both sides of the boundary are expected to be fetched.

            Those tiles are needed in case we perform bilinear antialiasing,
            provide us with at least a pixel width of tolerance for errors in
            other parts of the code or the system.  There is a decent chance
            that some or all of those extra tiles will need to be fetched before
            long, so getting them into the cache sooner rather than later is
            likely to be beneficial on average.

            The tests are carried out per viewport height / width.

            Lists are created of the first tile along an axis that is expected to
            be in the viewport and for the number of tiles along an axis that
            are expected to be in the viewport.

            These lists are used for both the x and y axes, although alternative
            lists are created for the x axis when the viewport spans the dateline
            and for the y axis when the viewport is clipped by the top or bottom of
            the map.

            These 5 areas are checked at an integral zoom level to see that the
            expected visible tiles match the actual visible tiles generated by
            the code under test.

            After that a fractional zoom level is set and the width and height of
            the viewport are scaled such that the same tiles should be visible,
            and the tests are repeated.
        */

    // TODO
    // nail down semantics, modify tests and code to suite
    // add corners of the map

    /*
            width = t - 1
        */

    QList<int> mid_tm1x;
    QList<int> mid_tm1w;
    QList<int> top_tm1x;
    QList<int> top_tm1w;
    QList<int> bottom_tm1x;
    QList<int> bottom_tm1w;
    QList<int> left_tm1x;
    QList<int> left_tm1w;
    QList<int> right_tm1x;
    QList<int> right_tm1w;

    pti.w = t - 1;
    pti.h = t - 1;
    pti.wString = QStringLiteral("(1T - 1)");
    pti.hString = QStringLiteral("(1T - 1)");

    /*

                offset = 0
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +    * *+* *    +       +       +
                +       +    *  +  *    +       +       +
                + + + + + + +*+ T T*T T T + + + + + + + +
                +       +    *  T  *    T       +       +
                +       +    * *T* *    T       +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 1
                Covers:     2 tiles
        */

    mid_tm1x << 7;
    mid_tm1w << 2;

    top_tm1x << 0;
    top_tm1w << 1;

    bottom_tm1x << 14;
    bottom_tm1w << 2;

    left_tm1x << 15;
    left_tm1w << 2;

    right_tm1x << 14;
    right_tm1w << 2;

    /*
                offset = 1
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +      *+* * *  +       +       +
                +       +      *+    *  +       +       +
                + + + + + + + +*T T T*T T + + + + + + + +
                +       +      *T    *  T       +       +
                +       +      *T* * *  T       +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 1
                Covers:     2 tiles
        */

    mid_tm1x << 7;
    mid_tm1w << 2;

    top_tm1x << 0;
    top_tm1w << 1;

    bottom_tm1x << 14;
    bottom_tm1w << 2;

    left_tm1x << 15;
    left_tm1w << 2;

    right_tm1x << 14;
    right_tm1w << 2;

    /*
                offset = 2
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +       +* * * *+       +       +
                +       +       +*     *+       +       +
                + + + + + + + + T*T T T*T + + + + + + + +
                +       +       T*     *T       +       +
                +       +       T* * * *T       +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T
                Covers:     1 tile
        */

    mid_tm1x << 8;
    mid_tm1w << 1;

    top_tm1x << 0;
    top_tm1w << 1;

    bottom_tm1x << 15;
    bottom_tm1w << 1;

    left_tm1x << 0;
    left_tm1w << 1;

    right_tm1x << 15;
    right_tm1w << 1;

    /*
                offset = 3
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +       +  * * *+*      +       +
                +       +       +  *    +*      +       +
                + + + + + + + + T T*T T T*+ + + + + + + +
                +       +       T  *    T*      +       +
                +       +       T  * * *T*      +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T
                Covers:     2 tiles
        */

    mid_tm1x << 8;
    mid_tm1w << 2;

    top_tm1x << 0;
    top_tm1w << 2;

    bottom_tm1x << 15;
    bottom_tm1w << 1;

    left_tm1x << 0;
    left_tm1w << 2;

    right_tm1x << 15;
    right_tm1w << 2;

    /*
                offset = 4
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +       +    * *+* *    +       +
                +       +       +    *  +  *    +       +
                + + + + + + + + T T T*T T +*+ + + + + + +
                +       +       T    *  T  *    +       +
                +       +       T    * *T* *    +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T
                Covers:     2 tiles
        */

    mid_tm1x << 8;
    mid_tm1w << 2;

    top_tm1x << 0;
    top_tm1w << 2;

    bottom_tm1x << 15;
    bottom_tm1w << 1;

    left_tm1x << 0;
    left_tm1w << 2;

    right_tm1x << 15;
    right_tm1w << 2;

    pti.zoom = 4.0;
    pti.zoomString = QStringLiteral("int zoom");

    pti.x = 0.5;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("middle");

    test_group(pti, mid_tm1x, mid_tm1w, mid_tm1x, mid_tm1w);

    pti.x = 0.5;
    pti.y = 0.0;
    pti.xyString = QStringLiteral("top");

    test_group(pti, mid_tm1x, mid_tm1w, top_tm1x, top_tm1w);

    pti.x = 0.5;
    pti.y = 15.0 / 16.0;
    pti.xyString = QStringLiteral("bottom");

    test_group(pti, mid_tm1x, mid_tm1w, bottom_tm1x, bottom_tm1w);

    pti.x = 0.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("left");

    test_group(pti, left_tm1x, left_tm1w, mid_tm1x, mid_tm1w);

    pti.x = 15.0 / 16.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("right");

    test_group(pti, right_tm1x, right_tm1w, mid_tm1x, mid_tm1w);

    pti.zoom = 4.5;
    pti.zoomString = QStringLiteral("frac zoom");
    pti.w = pti.w * qPow(2.0, 0.5);
    pti.h = pti.h * qPow(2.0, 0.5);

    pti.x = 0.5;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("middle");

    test_group(pti, mid_tm1x, mid_tm1w, mid_tm1x, mid_tm1w);

    pti.x = 0.5;
    pti.y = 0.0;
    pti.xyString = QStringLiteral("top");

    test_group(pti, mid_tm1x, mid_tm1w, top_tm1x, top_tm1w);

    pti.x = 0.5;
    pti.y = 15.0 / 16.0;
    pti.xyString = QStringLiteral("bottom");

    test_group(pti, mid_tm1x, mid_tm1w, bottom_tm1x, bottom_tm1w);

    pti.x = 0.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("left");

    test_group(pti, left_tm1x, left_tm1w, mid_tm1x, mid_tm1w);

    pti.x = 15.0 / 16.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("right");

    test_group(pti, right_tm1x, right_tm1w, mid_tm1x, mid_tm1w);

    /*
            width = t
        */

    QList<int> mid_tx;
    QList<int> mid_tw;
    QList<int> top_tx;
    QList<int> top_tw;
    QList<int> bottom_tx;
    QList<int> bottom_tw;
    QList<int> left_tx;
    QList<int> left_tw;
    QList<int> right_tx;
    QList<int> right_tw;

    pti.w = t;
    pti.h = t;
    pti.wString = QStringLiteral("1T");
    pti.hString = QStringLiteral("1T");

    /*

                offset = 0
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +   * * * * *   +       +       +
                +       +   *   +   *   +       +       +
                + + + + + + * + T T O T T + + + + + + + +
                +       +   *   T   *   T       +       +
                +       +   * * O * *   T       +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 1
                Covers:     2 tiles
        */

    mid_tx << 7;
    mid_tw << 2;

    top_tx << 0;
    top_tw << 1;

    bottom_tx << 14;
    bottom_tw << 2;

    left_tx << 15;
    left_tw << 2;

    right_tx << 14;
    right_tw << 2;

    /*
                offset = 1
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +     * * * * * +       +       +
                +       +     * +     * +       +       +
                + + + + + + + * T T T O T + + + + + + + +
                +       +     * T     * T       +       +
                +       +     * O * * * T       +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 1
                Covers:     2 tiles
        */

    mid_tx << 7;
    mid_tw << 2;

    top_tx << 0;
    top_tw << 1;

    bottom_tx << 14;
    bottom_tw << 2;

    left_tx << 15;
    left_tw << 2;

    right_tx << 14;
    right_tw << 2;

    /*
                offset = 2
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +       * * * * *       +       +
                +       +       *       *       +       +
                + + + + + + + + O T T T O + + + + + + + +
                +       +       O       O       +       +
                +       +       O * * * O       +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 1
                Covers:     3 tiles
        */

    mid_tx << 7;
    mid_tw << 3;

    top_tx << 0;
    top_tw << 2;

    bottom_tx << 14;
    bottom_tw << 2;

    left_tx << 15;
    left_tw << 3;

    right_tx << 14;
    right_tw << 3;

    /*
                offset = 3
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +       + * * * * *     +       +
                +       +       + *       *     +       +
                + + + + + + + + T O T T T * + + + + + + +
                +       +       T *     T *     +       +
                +       +       T * * * O *     +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T
                Covers:     2 tiles
        */

    mid_tx << 8;
    mid_tw << 2;

    top_tx << 0;
    top_tw << 2;

    bottom_tx << 15;
    bottom_tw << 1;

    left_tx << 0;
    left_tw << 2;

    right_tx << 15;
    right_tw << 2;

    /*
                offset = 4
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +       +   * * * * *   +       +
                +       +       +   *   +   *   +       +
                + + + + + + + + T T O T T + * + + + + + +
                +       +       T   *   T   *   +       +
                +       +       T   * * O * *   +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T
                Covers:     2 tiles
        */

    mid_tx << 8;
    mid_tw << 2;

    top_tx << 0;
    top_tw << 2;

    bottom_tx << 15;
    bottom_tw << 1;

    left_tx << 0;
    left_tw << 2;

    right_tx << 15;
    right_tw << 2;

    pti.zoom = 4.0;
    pti.zoomString = QStringLiteral("int zoom");

    pti.x = 0.5;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("middle");

    test_group(pti, mid_tx, mid_tw, mid_tx, mid_tw);

    pti.x = 0.5;
    pti.y = 0.0;
    pti.xyString = QStringLiteral("top");

    test_group(pti, mid_tx, mid_tw, top_tx, top_tw);

    pti.x = 0.5;
    pti.y = 15.0 / 16.0;
    pti.xyString = QStringLiteral("bottom");

    test_group(pti, mid_tx, mid_tw, bottom_tx, bottom_tw);

    pti.x = 0.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("left");

    test_group(pti, left_tx, left_tw, mid_tx, mid_tw);

    pti.x = 15.0 / 16.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("right");

    test_group(pti, right_tx, right_tw, mid_tx, mid_tw);

    pti.zoom = 4.5;
    pti.zoomString = QStringLiteral("frac zoom");
    pti.w = pti.w * qPow(2.0, 0.5);
    pti.h = pti.h * qPow(2.0, 0.5);

    pti.x = 0.5;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("middle");

    test_group(pti, mid_tx, mid_tw, mid_tx, mid_tw);

    pti.x = 0.5;
    pti.y = 0.0;
    pti.xyString = QStringLiteral("top");

    test_group(pti, mid_tx, mid_tw, top_tx, top_tw);

    pti.x = 0.5;
    pti.y = 15.0 / 16.0;
    pti.xyString = QStringLiteral("bottom");

    test_group(pti, mid_tx, mid_tw, bottom_tx, bottom_tw);

    pti.x = 0.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("left");

    test_group(pti, left_tx, left_tw, mid_tx, mid_tw);

    pti.x = 15.0 / 16.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("right");

    test_group(pti, right_tx, right_tw, mid_tx, mid_tw);

    /*
            width = t + 1
        */

    QList<int> mid_tp1x;
    QList<int> mid_tp1w;
    QList<int> top_tp1x;
    QList<int> top_tp1w;
    QList<int> bottom_tp1x;
    QList<int> bottom_tp1w;
    QList<int> left_tp1x;
    QList<int> left_tp1w;
    QList<int> right_tp1x;
    QList<int> right_tp1w;

    pti.w = t + 1;
    pti.h = t + 1;
    pti.wString = QStringLiteral("(1T + 1)");
    pti.hString = QStringLiteral("(1T + 1)");

    /*

                offset = 0
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +  * * *+* * *  +       +       +
                +       +  *    +    *  +       +       +
                + + + + + +*+ + T T T*T T + + + + + + + +
                +       +  *    T    *  T       +       +
                +       +  * * *T* * *  T       +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 1
                Covers:     2 tiles
        */

    mid_tp1x << 7;
    mid_tp1w << 2;

    top_tp1x << 0;
    top_tp1w << 1;

    bottom_tp1x << 14;
    bottom_tp1w << 2;

    left_tp1x << 15;
    left_tp1w << 2;

    right_tp1x << 14;
    right_tp1w << 2;

    /*
                offset = 1
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +    * *+* * * *+       +       +
                +       +    *  +      *+       +       +
                + + + + + + +*+ T T T T*T + + + + + + + +
                +       +    *  T      *T       +       +
                +       +    * *T* * * *T       +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 1
                Covers:     2 tiles
        */

    mid_tp1x << 7;
    mid_tp1w << 2;

    top_tp1x << 0;
    top_tp1w << 1;

    bottom_tp1x << 14;
    bottom_tp1w << 2;

    left_tp1x << 15;
    left_tp1w << 2;

    right_tp1x << 14;
    right_tp1w << 2;

    /*
                offset = 2
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +      *+* * * *+*      +       +
                +       +      *+       +*      +       +
                + + + + + + + +*T T T T T*+ + + + + + + +
                +       +      *T       T*      +       +
                +       +      *T* * * *T*      +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 1
                Covers:     3 tiles
        */

    mid_tp1x << 7;
    mid_tp1w << 3;

    top_tp1x << 0;
    top_tp1w << 2;

    bottom_tp1x << 14;
    bottom_tp1w << 2;

    left_tp1x << 15;
    left_tp1w << 3;

    right_tp1x << 14;
    right_tp1w << 3;

    /*
                offset = 3
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +       +* * * *+* *    +       +
                +       +       +*      +  *    +       +
                + + + + + + + + T*T T T T +*+ + + + + + +
                +       +       T*      T  *    +       +
                +       +       T* * * *T* *    +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T
                Covers:     2 tiles
        */

    mid_tp1x << 8;
    mid_tp1w << 2;

    top_tp1x << 0;
    top_tp1w << 2;

    bottom_tp1x << 15;
    bottom_tp1w << 1;

    left_tp1x << 0;
    left_tp1w << 2;

    right_tp1x << 15;
    right_tp1w << 2;

    /*
                offset = 4
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +       +  * * *+* * *  +       +
                +       +       +  *    +    *  +       +
                + + + + + + + + T T*T T T + +*+ + + + + +
                +       +       T  *    T    *  +       +
                +       +       T  * * *T* * *  +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T
                Covers:     2 tiles
        */

    mid_tp1x << 8;
    mid_tp1w << 2;

    top_tp1x << 0;
    top_tp1w << 2;

    bottom_tp1x << 15;
    bottom_tp1w << 1;

    left_tp1x << 0;
    left_tp1w << 2;

    right_tp1x << 15;
    right_tp1w << 2;

    pti.zoom = 4.0;
    pti.zoomString = QStringLiteral("int zoom");

    pti.x = 0.5;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("middle");

    test_group(pti, mid_tp1x, mid_tp1w, mid_tp1x, mid_tp1w);

    pti.x = 0.5;
    pti.y = 0.0;
    pti.xyString = QStringLiteral("top");

    test_group(pti, mid_tp1x, mid_tp1w, top_tp1x, top_tp1w);

    pti.x = 0.5;
    pti.y = 15.0 / 16.0;
    pti.xyString = QStringLiteral("bottom");

    test_group(pti, mid_tp1x, mid_tp1w, bottom_tp1x, bottom_tp1w);

    pti.x = 0.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("left");

    test_group(pti, left_tp1x, left_tp1w, mid_tp1x, mid_tp1w);

    pti.x = 15.0 / 16.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("right");

    test_group(pti, right_tp1x, right_tp1w, mid_tp1x, mid_tp1w);

    pti.zoom = 4.5;
    pti.zoomString = QStringLiteral("frac zoom");
    pti.w = pti.w * qPow(2.0, 0.5);
    pti.h = pti.h * qPow(2.0, 0.5);

    pti.x = 0.5;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("middle");

    test_group(pti, mid_tp1x, mid_tp1w, mid_tp1x, mid_tp1w);

    pti.x = 0.5;
    pti.y = 0.0;
    pti.xyString = QStringLiteral("top");

    test_group(pti, mid_tp1x, mid_tp1w, top_tp1x, top_tp1w);

    pti.x = 0.5;
    pti.y = 15.0 / 16.0;
    pti.xyString = QStringLiteral("bottom");

    test_group(pti, mid_tp1x, mid_tp1w, bottom_tp1x, bottom_tp1w);

    pti.x = 0.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("left");

    test_group(pti, left_tp1x, left_tp1w, mid_tp1x, mid_tp1w);

    pti.x = 15.0 / 16.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("right");

    test_group(pti, right_tp1x, right_tp1w, mid_tp1x, mid_tp1w);

    /*
            width = 2t - 1
        */

    QList<int> mid_t2m1x;
    QList<int> mid_t2m1w;
    QList<int> top_t2m1x;
    QList<int> top_t2m1w;
    QList<int> bottom_t2m1x;
    QList<int> bottom_t2m1w;
    QList<int> left_t2m1x;
    QList<int> left_t2m1w;
    QList<int> right_t2m1x;
    QList<int> right_t2m1w;

    pti.w = 2 * t - 1;
    pti.h = 2 * t - 1;
    pti.wString = QStringLiteral("(2T - 1)");
    pti.hString = QStringLiteral("(2T - 1)");

    /*
                offset = 0
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +* * * *+* * * *+       +       +
                +       +*      +      *+       +       +
                + + + + +*+ + + T T T T*T + + + + + + + +
                +       +*      T      *T       +       +
                +       +* * * *T* * * *T       +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 1
                Covers:     2 tiles
        */

    mid_t2m1x << 7;
    mid_t2m1w << 2;

    top_t2m1x << 0;
    top_t2m1w << 1;

    bottom_t2m1x << 14;
    bottom_t2m1w << 2;

    left_t2m1x << 15;
    left_t2m1w << 2;

    right_t2m1x << 14;
    right_t2m1w << 2;

    /*
                offset = 1
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +  * * *+* * * *+*      +       +
                +       +  *    +       +*      +       +
                + + + + + +*+ + T T T T T*+ + + + + + + +
                +       +  *    T       T*      +       +
                +       +  * * *T* * * *T*      +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 1
                Covers:     3 tiles
        */

    mid_t2m1x << 7;
    mid_t2m1w << 3;

    top_t2m1x << 0;
    top_t2m1w << 2;

    bottom_t2m1x << 14;
    bottom_t2m1w << 2;

    left_t2m1x << 15;
    left_t2m1w << 3;

    right_t2m1x << 14;
    right_t2m1w << 3;

    /*
                offset = 2
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +    * *+* * * *+* *    +       +
                +       +    *  +       +  *    +       +
                + + + + + + +*+ T T T T T +*+ + + + + + +
                +       +    *  T       T  *    +       +
                +       +    * *T* * * *T* *    +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 1
                Covers:     3 tiles
        */

    mid_t2m1x << 7;
    mid_t2m1w << 3;

    top_t2m1x << 0;
    top_t2m1w << 2;

    bottom_t2m1x << 14;
    bottom_t2m1w << 2;

    left_t2m1x << 15;
    left_t2m1w << 3;

    right_t2m1x << 14;
    right_t2m1w << 3;

    /*
                offset = 3
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +      *+* * * *+* * *  +       +
                +       +      *+       +    *  +       +
                + + + + + + + +*T T T T T + +*+ + + + + +
                +       +      *T       T    *  +       +
                +       +      *T* * * *T* * *  +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 1
                Covers:     3 tiles
        */

    mid_t2m1x << 7;
    mid_t2m1w << 3;

    top_t2m1x << 0;
    top_t2m1w << 2;

    bottom_t2m1x << 14;
    bottom_t2m1w << 2;

    left_t2m1x << 15;
    left_t2m1w << 3;

    right_t2m1x << 14;
    right_t2m1w << 3;

    /*
                offset = 4
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +       +* * * *+* * * *+       +
                +       +       +*      +      *+       +
                + + + + + + + + T*T T T T + + +*+ + + + +
                +       +       T*      T      *+       +
                +       +       T* * * *T* * * *+       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T
                Covers:     2 tiles
        */

    mid_t2m1x << 8;
    mid_t2m1w << 2;

    top_t2m1x << 0;
    top_t2m1w << 2;

    bottom_t2m1x << 15;
    bottom_t2m1w << 1;

    left_t2m1x << 0;
    left_t2m1w << 2;

    right_t2m1x << 15;
    right_t2m1w << 2;

    pti.zoom = 4.0;
    pti.zoomString = QStringLiteral("int zoom");

    pti.x = 0.5;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("middle");

    test_group(pti, mid_t2m1x, mid_t2m1w, mid_t2m1x, mid_t2m1w);

    pti.x = 0.5;
    pti.y = 0.0;
    pti.xyString = QStringLiteral("top");

    test_group(pti, mid_t2m1x, mid_t2m1w, top_t2m1x, top_t2m1w);

    pti.x = 0.5;
    pti.y = 15.0 / 16.0;
    pti.xyString = QStringLiteral("bottom");

    test_group(pti, mid_t2m1x, mid_t2m1w, bottom_t2m1x, bottom_t2m1w);

    pti.x = 0.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("left");

    test_group(pti, left_t2m1x, left_t2m1w, mid_t2m1x, mid_t2m1w);

    pti.x = 15.0 / 16.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("right");

    test_group(pti, right_t2m1x, right_t2m1w, mid_t2m1x, mid_t2m1w);

    pti.zoom = 4.5;
    pti.zoomString = QStringLiteral("frac zoom");
    pti.w = pti.w * qPow(2.0, 0.5);
    pti.h = pti.h * qPow(2.0, 0.5);

    pti.x = 0.5;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("middle");

    test_group(pti, mid_t2m1x, mid_t2m1w, mid_t2m1x, mid_t2m1w);

    pti.x = 0.5;
    pti.y = 0.0;
    pti.xyString = QStringLiteral("top");

    test_group(pti, mid_t2m1x, mid_t2m1w, top_t2m1x, top_t2m1w);

    pti.x = 0.5;
    pti.y = 15.0 / 16.0;
    pti.xyString = QStringLiteral("bottom");

    test_group(pti, mid_t2m1x, mid_t2m1w, bottom_t2m1x, bottom_t2m1w);

    pti.x = 0.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("left");

    test_group(pti, left_t2m1x, left_t2m1w, mid_t2m1x, mid_t2m1w);

    pti.x = 15.0 / 16.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("right");

    test_group(pti, right_t2m1x, right_t2m1w, mid_t2m1x, mid_t2m1w);

    /*
            width = 2t
        */


    QList<int> mid_t2x;
    QList<int> mid_t2w;
    QList<int> top_t2x;
    QList<int> top_t2w;
    QList<int> bottom_t2x;
    QList<int> bottom_t2w;
    QList<int> left_t2x;
    QList<int> left_t2w;
    QList<int> right_t2x;
    QList<int> right_t2w;

    pti.w = 2 * t;
    pti.h = 2 * t;
    pti.wString = QStringLiteral("2T");
    pti.hString = QStringLiteral("2T");

    /*

                offset = 0
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       * * * * * * * * *       +       +
                +       *       +       *       +       +
                + + + + * + + + T T T T O + + + + + + + +
                +       *       T       O       +       +
                +       * * * * O * * * O       +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 2
                Covers:     4 tiles
        */

    mid_t2x << 6;
    mid_t2w << 4;

    top_t2x << 0;
    top_t2w << 2;

    bottom_t2x << 13;
    bottom_t2w << 3;

    left_t2x << 14;
    left_t2w << 4;

    right_t2x << 13;
    right_t2w << 4;

    /*
                offset = 1
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       + * * * * * * * * *     +       +
                +       + *     +         *     +       +
                + + + + + * + + T T T T T * + + + + + + +
                +       + *     T       T *     +       +
                +       + * * * O * * * O *     +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 1
                Covers:     3 tiles
        */

    mid_t2x << 7;
    mid_t2w << 3;

    top_t2x << 0;
    top_t2w << 2;

    bottom_t2x << 14;
    bottom_t2w << 2;

    left_t2x << 15;
    left_t2w << 3;

    right_t2x << 14;
    right_t2w << 3;

    /*
                offset = 2
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +   * * * * * * * * *   +       +
                +       +   *   +           *   +       +
                + + + + + + * + T T T T T + * + + + + + +
                +       +   *   T       T   *   +       +
                +       +   * * O * * * O * *   +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 1
                Covers:     3 tiles
        */

    mid_t2x << 7;
    mid_t2w << 3;

    top_t2x << 0;
    top_t2w << 2;

    bottom_t2x << 14;
    bottom_t2w << 2;

    left_t2x << 15;
    left_t2w << 3;

    right_t2x << 14;
    right_t2w << 3;

    /*
                offset = 3
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +     * * * * * * * * * +       +
                +       +     * +             * +       +
                + + + + + + + * T T T T T + + * + + + + +
                +       +     * T       T     * +       +
                +       +     * O * * * O * * * +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 1
                Covers:     3 tiles
        */

    mid_t2x << 7;
    mid_t2w << 3;

    top_t2x << 0;
    top_t2w << 2;

    bottom_t2x << 14;
    bottom_t2w << 2;

    left_t2x << 15;
    left_t2w << 3;

    right_t2x << 14;
    right_t2w << 3;

    /*
                offset = 4
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +       * * * * * * * * *       +
                +       +       *               *       +
                + + + + + + + + O T T T T + + + * + + + +
                +       +       O       T       *       +
                +       +       O * * * O * * * *       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 1
                Covers:     4 tiles
        */

    mid_t2x << 7;
    mid_t2w << 4;

    top_t2x << 0;
    top_t2w << 3;

    bottom_t2x << 14;
    bottom_t2w << 2;

    left_t2x << 15;
    left_t2w << 4;

    right_t2x << 14;
    right_t2w << 4;

    pti.zoom = 4.0;
    pti.zoomString = QStringLiteral("int zoom");

    pti.x = 0.5;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("middle");

    test_group(pti, mid_t2x, mid_t2w, mid_t2x, mid_t2w);

    pti.x = 0.5;
    pti.y = 0.0;
    pti.xyString = QStringLiteral("top");

    test_group(pti, mid_t2x, mid_t2w, top_t2x, top_t2w);

    pti.x = 0.5;
    pti.y = 15.0 / 16.0;
    pti.xyString = QStringLiteral("bottom");

    test_group(pti, mid_t2x, mid_t2w, bottom_t2x, bottom_t2w);

    pti.x = 0.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("left");

    test_group(pti, left_t2x, left_t2w, mid_t2x, mid_t2w);

    pti.x = 15.0 / 16.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("right");

    test_group(pti, right_t2x, right_t2w, mid_t2x, mid_t2w);

    pti.zoom = 4.5;
    pti.zoomString = QStringLiteral("frac zoom");
    pti.w = pti.w * qPow(2.0, 0.5);
    pti.h = pti.h * qPow(2.0, 0.5);

    pti.x = 0.5;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("middle");

    test_group(pti, mid_t2x, mid_t2w, mid_t2x, mid_t2w);

    pti.x = 0.5;
    pti.y = 0.0;
    pti.xyString = QStringLiteral("top");

    test_group(pti, mid_t2x, mid_t2w, top_t2x, top_t2w);

    pti.x = 0.5;
    pti.y = 15.0 / 16.0;
    pti.xyString = QStringLiteral("bottom");

    test_group(pti, mid_t2x, mid_t2w, bottom_t2x, bottom_t2w);

    pti.x = 0.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("left");

    test_group(pti, left_t2x, left_t2w, mid_t2x, mid_t2w);

    pti.x = 15.0 / 16.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("right");

    test_group(pti, right_t2x, right_t2w, mid_t2x, mid_t2w);

    /*
            width = 2t + 1
        */

    QList<int> mid_t2p1x;
    QList<int> mid_t2p1w;
    QList<int> top_t2p1x;
    QList<int> top_t2p1w;
    QList<int> bottom_t2p1x;
    QList<int> bottom_t2p1w;
    QList<int> left_t2p1x;
    QList<int> left_t2p1w;
    QList<int> right_t2p1x;
    QList<int> right_t2p1w;

    pti.w = 2 * t + 1;
    pti.h = 2 * t + 1;
    pti.wString = QStringLiteral("(2T + 1)");
    pti.hString = QStringLiteral("(2T + 1)");

    /*

                offset = 0
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +      *+* * * *+* * * *+*      +       +
                +      *+       +       +*      +       +
                + + + +*+ + + + T T T T T*+ + + + + + + +
                +      *+       T       T*      +       +
                +      *+* * * *T* * * *T*      +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 2
                Covers:     4 tiles
        */

    mid_t2p1x << 6;
    mid_t2p1w << 4;

    top_t2p1x << 0;
    top_t2p1w << 2;

    bottom_t2p1x << 13;
    bottom_t2p1w << 3;

    left_t2p1x << 14;
    left_t2p1w << 4;

    right_t2p1x << 13;
    right_t2p1w << 4;

    /*
                offset = 1
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +* * * *+* * * *+* *    +       +
                +       +*      +       +  *    +       +
                + + + + +*+ + + T T T T T +*+ + + + + + +
                +       +*      T       T  *    +       +
                +       +* * * *T* * * *T* *    +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 1
                Covers:     3 tiles
        */

    mid_t2p1x << 7;
    mid_t2p1w << 3;

    top_t2p1x << 0;
    top_t2p1w << 2;

    bottom_t2p1x << 14;
    bottom_t2p1w << 2;

    left_t2p1x << 15;
    left_t2p1w << 3;

    right_t2p1x << 14;
    right_t2p1w << 3;

    /*
                offset = 2
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +  * * *+* * * *+* * *  +       +
                +       +  *    +       +    *  +       +
                + + + + + +*+ + T T T T T + +*+ + + + + +
                +       +  *    T       T    *  +       +
                +       +  * * *T* * * *T* * *  +       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 1
                Covers:     3 tiles
        */

    mid_t2p1x << 7;
    mid_t2p1w << 3;

    top_t2p1x << 0;
    top_t2p1w << 2;

    bottom_t2p1x << 14;
    bottom_t2p1w << 2;

    left_t2p1x << 15;
    left_t2p1w << 3;

    right_t2p1x << 14;
    right_t2p1w << 3;

    /*
                offset = 3
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +    * *+* * * *+* * * *+       +
                +       +    *  +       +      *+       +
                + + + + + + +*+ T T T T T + + +*+ + + + +
                +       +    *  T       T      *+       +
                +       +    * *T* * * *T* * * *+       +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 1
                Covers:     3 tiles
        */

    mid_t2p1x << 7;
    mid_t2p1w << 3;

    top_t2p1x << 0;
    top_t2p1w << 2;

    bottom_t2p1x << 14;
    bottom_t2p1w << 2;

    left_t2p1x << 15;
    left_t2p1w << 3;

    right_t2p1x << 14;
    right_t2p1w << 3;

    /*
                offset = 4
                + + + + + + + + + + + + + + + + + + + + +
                +       +       +       +       +       +
                +       +      *+* * * *+* * * *+*      +
                +       +      *+       +       +*      +
                + + + + + + + +*T T T T T + + + +*+ + + +
                +       +      *T       T       +*      +
                +       +      *T* * * *T* * * *+*      +
                +       +       T       T       +       +
                + + + + + + + + T T T T T + + + + + + + +

                Starts at:  T - 1
                Covers:     4 tiles
         */

    mid_t2p1x << 7;
    mid_t2p1w << 4;

    top_t2p1x << 0;
    top_t2p1w << 3;

    bottom_t2p1x << 14;
    bottom_t2p1w << 2;

    left_t2p1x << 15;
    left_t2p1w << 4;

    right_t2p1x << 14;
    right_t2p1w << 4;

    pti.zoom = 4.0;
    pti.zoomString = QStringLiteral("int zoom");

    pti.x = 0.5;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("middle");

    test_group(pti, mid_t2p1x, mid_t2p1w, mid_t2p1x, mid_t2p1w);

    pti.x = 0.5;
    pti.y = 0.0;
    pti.xyString = QStringLiteral("top");

    test_group(pti, mid_t2p1x, mid_t2p1w, top_t2p1x, top_t2p1w);

    pti.x = 0.5;
    pti.y = 15.0 / 16.0;
    pti.xyString = QStringLiteral("bottom");

    test_group(pti, mid_t2p1x, mid_t2p1w, bottom_t2p1x, bottom_t2p1w);

    pti.x = 0.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("left");

    test_group(pti, left_t2p1x, left_t2p1w, mid_t2p1x, mid_t2p1w);

    pti.x = 15.0 / 16.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("right");

    test_group(pti, right_t2p1x, right_t2p1w, mid_t2p1x, mid_t2p1w);

    pti.zoom = 4.5;
    pti.zoomString = QStringLiteral("frac zoom");
    pti.w = pti.w * qPow(2.0, 0.5);
    pti.h = pti.h * qPow(2.0, 0.5);

    pti.x = 0.5;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("middle");

    test_group(pti, mid_t2p1x, mid_t2p1w, mid_t2p1x, mid_t2p1w);

    pti.x = 0.5;
    pti.y = 0.0;
    pti.xyString = QStringLiteral("top");

    test_group(pti, mid_t2p1x, mid_t2p1w, top_t2p1x, top_t2p1w);

    pti.x = 0.5;
    pti.y = 15.0 / 16.0;
    pti.xyString = QStringLiteral("bottom");

    test_group(pti, mid_t2p1x, mid_t2p1w, bottom_t2p1x, bottom_t2p1w);

    pti.x = 0.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("left");

    test_group(pti, left_t2p1x, left_t2p1w, mid_t2p1x, mid_t2p1w);

    pti.x = 15.0 / 16.0;
    pti.y = 0.5;
    pti.xyString = QStringLiteral("right");

    test_group(pti, right_t2p1x, right_t2p1w, mid_t2p1x, mid_t2p1w);
}

QTEST_GUILESS_MAIN(tst_QGeoCameraTiles)
#include "tst_qgeocameratiles.moc"
