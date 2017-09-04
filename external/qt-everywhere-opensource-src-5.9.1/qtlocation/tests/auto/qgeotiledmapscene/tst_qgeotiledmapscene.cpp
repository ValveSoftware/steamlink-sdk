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
#include "qgeotiledmapscene_p.h"
#include "qgeocameratiles_p.h"
#include "qgeocameradata_p.h"
#include "qabstractgeotilecache_p.h"
#include <QtLocation/private/qgeoprojection_p.h>
#include <QtPositioning/private/qwebmercator_p.h>
#include <QtPositioning/private/qdoublevector2d_p.h>

#include <qtest.h>

#include <QList>
#include <QPair>
#include <QDebug>

#include <cmath>

QT_USE_NAMESPACE

class tst_QGeoTiledMapScene : public QObject
{
    Q_OBJECT

    private:
    void row(QString name, double screenX, double screenX2, double screenY, double cameraCenterX, double cameraCenterY,
             double zoom, int tileSize, int screenWidth, int screenHeight, double mercatorX, double mercatorY){

        // expected behaviour of wrapping
        if (mercatorX <= 0.0)
            mercatorX += 1.0;
        else if (mercatorX > 1.0)
            mercatorX -= 1.0;

        QTest::newRow(qPrintable(name))
                << screenX << screenX2 << screenY
                << cameraCenterX << cameraCenterY
                << zoom << tileSize
                << screenWidth << screenHeight
                << mercatorX
                << mercatorY;
    }

    void screenPositions(QString name, double cameraCenterX, double cameraCenterY,  double zoom,
                         int tileSize, int screenWidth, int screenHeight)
    {
        double screenX;
        double screenX2;
        double screenY;
        double mercatorX;
        double mercatorY;

        double halfLength = 1 / (std::pow(2.0, zoom) * 2);
        double scaleX = screenWidth / tileSize;
        double scaleY = screenHeight / tileSize;
        double scaledHalfLengthX = halfLength * scaleX;
        double scaledHalfLengthY = halfLength * scaleY;

        bool matchingMapEnds = false;
        if (screenWidth == std::pow(2.0, zoom) * tileSize)
            matchingMapEnds = true;

        // bottom left
        screenX = screenX2 = 0.0;
        if (matchingMapEnds)
            screenX2 = qAbs(screenX - 1.0) * screenWidth;
        screenY = 1.0 * screenHeight;
        mercatorX = cameraCenterX - scaledHalfLengthX;
        mercatorY = cameraCenterY + scaledHalfLengthY;
        row (name + QString("_bottomLeftScreen"), screenX, screenX2, screenY, cameraCenterX, cameraCenterY,
                     zoom, tileSize, screenWidth, screenHeight, mercatorX, mercatorY);

        // bottom
        screenX = screenX2 = 0.5 * screenWidth;
        screenY = 1.0 * screenHeight;
        mercatorX = cameraCenterX;
        mercatorY = cameraCenterY + scaledHalfLengthY;
        row (name + QString("_bottomScreen"), screenX, screenX2, screenY, cameraCenterX, cameraCenterY,
                     zoom, tileSize, screenWidth, screenHeight, mercatorX, mercatorY);

        // bottom right
        screenX = screenX2 = 1.0 * screenWidth;
        if (matchingMapEnds)
            screenX2 = qAbs(screenX - 1.0) * screenWidth;
        screenY = 1.0 * screenHeight;
        mercatorX = cameraCenterX + scaledHalfLengthX;
        mercatorY = cameraCenterY + scaledHalfLengthY;
        row (name + QString("_bottomRightScreen"), screenX, screenX2, screenY, cameraCenterX, cameraCenterY,
                     zoom, tileSize, screenWidth, screenHeight, mercatorX, mercatorY);

        // left
        screenX = screenX2 = 0.0 * screenWidth;
        if (matchingMapEnds)
            screenX2 = qAbs(screenX - 1.0) * screenWidth;
        screenY = 0.5 * screenHeight;
        mercatorX = cameraCenterX - scaledHalfLengthX;
        mercatorY = cameraCenterY;
        row (name + QString("_leftScreen"), screenX, screenX2, screenY, cameraCenterX, cameraCenterY,
                     zoom, tileSize, screenWidth, screenHeight, mercatorX, mercatorY);

        // center
        screenX = screenX2 = 0.5 * screenWidth;
        screenY = 0.5 * screenHeight;
        mercatorX = cameraCenterX;
        mercatorY = cameraCenterY;
        row (name + QString("_centerScreen"), screenX, screenX2, screenY, cameraCenterX, cameraCenterY,
                     zoom, tileSize, screenWidth, screenHeight, mercatorX, mercatorY);

        // right
        screenX = screenX2 = 1.0 * screenWidth;
        if (matchingMapEnds)
            screenX2 = qAbs(screenX - 1.0) * screenWidth;
        screenY = 0.5 * screenHeight;
        mercatorX = cameraCenterX + scaledHalfLengthX;
        mercatorY = cameraCenterY;
        row (name + QString("_rightScreen"), screenX, screenX2, screenY, cameraCenterX, cameraCenterY,
                     zoom, tileSize, screenWidth, screenHeight, mercatorX, mercatorY);

        // top left
        screenX = screenX2 = 0.0;
        if (matchingMapEnds)
            screenX2 = qAbs(screenX - 1.0) * screenWidth;
        screenY = 0.0;
        mercatorX = cameraCenterX - scaledHalfLengthX;
        mercatorY = cameraCenterY - scaledHalfLengthY;
        row (name + QString("_topLeftScreen"), screenX, screenX2, screenY, cameraCenterX, cameraCenterY,
                     zoom, tileSize, screenWidth, screenHeight, mercatorX, mercatorY);

        // top
        screenX = screenX2 = 0.5 * screenWidth;
        screenY = 0.0;
        mercatorX = cameraCenterX;
        mercatorY = cameraCenterY - scaledHalfLengthY;
        row (name + QString("_topScreen"), screenX, screenX2, screenY, cameraCenterX, cameraCenterY,
                     zoom, tileSize, screenWidth, screenHeight, mercatorX, mercatorY);

        // top right
        screenX = screenX2 = 1.0 * screenWidth;
        if (matchingMapEnds)
            screenX2 = qAbs(screenX - 1.0) * screenWidth;
        screenY = 0.0;
        mercatorX = cameraCenterX + scaledHalfLengthX;
        mercatorY = cameraCenterY - scaledHalfLengthY;
        row (name + QString("_topRightScreen"), screenX, screenX2, screenY, cameraCenterX, cameraCenterY,
                     zoom, tileSize, screenWidth, screenHeight, mercatorX, mercatorY);

    }

    void screenCameraPositions(QString name, double zoom, int tileSize,
                               int screenWidth, int screenHeight)
    {
        double cameraCenterX;
        double cameraCenterY;

        // bottom left
        cameraCenterX = 0;
        cameraCenterY = 1.0;
        screenPositions(name + QString("_bottomLeftCamera"), cameraCenterX, cameraCenterY,
                        zoom, tileSize, screenWidth, screenHeight);

        // bottom
        cameraCenterX = 0.5;
        cameraCenterY = 1.0;
        screenPositions(name + QString("_bottomCamera"), cameraCenterX, cameraCenterY,
                        zoom, tileSize, screenWidth, screenHeight);
        // bottom right
        cameraCenterX = 1.0;
        cameraCenterY = 1.0;
        screenPositions(name + QString("_bottomRightCamera"), cameraCenterX, cameraCenterY,
                        zoom, tileSize, screenWidth, screenHeight);
        // left
        cameraCenterX = 0.0;
        cameraCenterY = 0.5;
        screenPositions(name + QString("_leftCamera"), cameraCenterX, cameraCenterY,
                        zoom, tileSize, screenWidth, screenHeight);
        // middle
        cameraCenterX = 0.5;
        cameraCenterY = 0.5;
        screenPositions(name + QString("_middleCamera"), cameraCenterX, cameraCenterY,
                        zoom, tileSize, screenWidth, screenHeight);
        // right
        cameraCenterX = 1.0;
        cameraCenterY = 0.5;
        screenPositions(name + QString("_rightCamera"), cameraCenterX, cameraCenterY,
                        zoom, tileSize, screenWidth, screenHeight);
        // top left
        cameraCenterX = 0.0;
        cameraCenterY = 0.0;
        screenPositions(name + QString("_topLeftCamera"), cameraCenterX, cameraCenterY,
                        zoom, tileSize, screenWidth, screenHeight);
        // top
        cameraCenterX = 0.5;
        cameraCenterY = 0.0;
        screenPositions(name + QString("_topCamera"), cameraCenterX, cameraCenterY,
                        zoom, tileSize, screenWidth, screenHeight);
        // top right
        cameraCenterX = 1.0;
        cameraCenterY = 0.0;
        screenPositions(name + QString("_topRightCamera"), cameraCenterX, cameraCenterY,
                        zoom, tileSize, screenWidth, screenHeight);
    }

    void populateScreenMercatorData(){
        QTest::addColumn<double>("screenX");
        QTest::addColumn<double>("screenX2");
        QTest::addColumn<double>("screenY");
        QTest::addColumn<double>("cameraCenterX");
        QTest::addColumn<double>("cameraCenterY");
        QTest::addColumn<double>("zoom");
        QTest::addColumn<int>("tileSize");
        QTest::addColumn<int>("screenWidth");
        QTest::addColumn<int>("screenHeight");
        QTest::addColumn<double>("mercatorX");
        QTest::addColumn<double>("mercatorY");

        int tileSize;
        double zoom;
        int screenWidth;
        int screenHeight;
        QString name;
        tileSize = 256;
        zoom = 1.0; // 4 tiles in the map. map size =  2*tileSize x 2*tileSize

        /*
            ScreenWidth = t
            ScreenHeight = t
        */
        screenWidth = tileSize;
        screenHeight = tileSize;
        name = QString("_(t x t)");
        screenCameraPositions(name, zoom, tileSize, screenWidth, screenHeight);

        /*
            ScreenWidth = t * 2
            ScreenHeight = t
        */
        screenWidth = tileSize * 2;
        screenHeight = tileSize;
        name = QString("_(2t x t)");
        screenCameraPositions(name, zoom, tileSize, screenWidth, screenHeight);

        /*
            ScreenWidth = t
            ScreenHeight = t * 2
        */
        screenWidth = tileSize;
        screenHeight = tileSize * 2;
        name = QString("_(2t x t)");
        screenCameraPositions(name, zoom, tileSize, screenWidth, screenHeight);


        /*
            Screen Width = t * 2
            Screen Height = t * 2
        */
        screenWidth = tileSize * 2;
        screenHeight = tileSize * 2;
        name = QString("_(2t x 2t)");
        screenCameraPositions(name, zoom, tileSize, screenWidth, screenHeight);
    }

    // Calculates the distance in mercator space of 2 x coordinates, assuming that 1 == 0
    double wrappedMercatorDistance(double x1, double x2)
    {
        return qMin(qMin(qAbs(x1 - 1.0 - x2), qAbs(x1 - x2)), qAbs(x1 + 1.0 - x2));
    }

    private slots:
        void screenToMercatorPositions(){
            QFETCH(double, screenX);
            QFETCH(double, screenY);
            QFETCH(double, cameraCenterX);
            QFETCH(double, cameraCenterY);
            QFETCH(double, zoom);
            QFETCH(int, tileSize);
            QFETCH(int, screenWidth);
            QFETCH(int, screenHeight);
            QFETCH(double, mercatorX);
            QFETCH(double, mercatorY);

            QGeoCameraData camera;
            camera.setZoomLevel(zoom);
            QGeoCoordinate centerCoordinate = QWebMercator::mercatorToCoord(QDoubleVector2D(cameraCenterX, cameraCenterY));
            camera.setCenter(centerCoordinate);

            QGeoCameraTiles ct;
            ct.setTileSize(tileSize);
            ct.setCameraData(camera);
            ct.setScreenSize(QSize(screenWidth,screenHeight));

            QGeoTiledMapScene mapGeometry;
            mapGeometry.setTileSize(tileSize);
            mapGeometry.setScreenSize(QSize(screenWidth,screenHeight));
            mapGeometry.setCameraData(camera);
            mapGeometry.setVisibleTiles(ct.createTiles());

            QGeoProjectionWebMercator projection;
            projection.setViewportSize(QSize(screenWidth,screenHeight));
            projection.setCameraData(camera);

            QDoubleVector2D point(screenX,screenY);
            QDoubleVector2D mercartorPos = projection.unwrapMapProjection(projection.itemPositionToWrappedMapProjection(point));

            const double tolerance = 0.00000000001; // FuzzyCompare is too strict here
            QVERIFY2(wrappedMercatorDistance(mercartorPos.x(),  mercatorX) < tolerance,
                                 qPrintable(QString("Accepted: %1 ,  Actual: %2")
                                            .arg(QString::number(mercatorX))
                                            .arg(QString::number(mercartorPos.x()))));
            QVERIFY2(qAbs(mercartorPos.y() - mercatorY) < tolerance,
                                 qPrintable(QString("Accepted: %1 ,  Actual: %2")
                                            .arg(QString::number(mercatorY))
                                            .arg(QString::number(mercartorPos.y()))));
        }

        void screenToMercatorPositions_data()
        {
            populateScreenMercatorData();
        }

        void mercatorToScreenPositions(){
            QFETCH(double, screenX);
            QFETCH(double, screenX2);
            QFETCH(double, screenY);
            QFETCH(double, cameraCenterX);
            QFETCH(double, cameraCenterY);
            QFETCH(double, zoom);
            QFETCH(int, tileSize);
            QFETCH(int, screenWidth);
            QFETCH(int, screenHeight);
            QFETCH(double, mercatorX);
            QFETCH(double, mercatorY);

            QGeoCameraData camera;
            camera.setZoomLevel(zoom);
            QGeoCoordinate coord = QWebMercator::mercatorToCoord(QDoubleVector2D(cameraCenterX, cameraCenterY));
            camera.setCenter(coord);

            QGeoCameraTiles ct;
            ct.setTileSize(tileSize);
            ct.setCameraData(camera);
            ct.setScreenSize(QSize(screenWidth,screenHeight));

            QGeoTiledMapScene mapGeometry;
            mapGeometry.setTileSize(tileSize);
            mapGeometry.setScreenSize(QSize(screenWidth,screenHeight));
            mapGeometry.setCameraData(camera);
            mapGeometry.setVisibleTiles(ct.createTiles());

            QGeoProjectionWebMercator projection;
            projection.setViewportSize(QSize(screenWidth,screenHeight));
            projection.setCameraData(camera);

            QDoubleVector2D mercatorPos(mercatorX, mercatorY);
            QPointF point = projection.wrappedMapProjectionToItemPosition(projection.wrapMapProjection(mercatorPos)).toPointF();

            QVERIFY2((point.x() == screenX) || (point.x() == screenX2),
                                 qPrintable(QString("Accepted: { %1 , %2 } Actual: %3")
                                            .arg(QString::number(screenX))
                                            .arg(QString::number(screenX2))
                                            .arg(QString::number(point.x()))));
            QCOMPARE(point.y(), screenY);
        }

        void mercatorToScreenPositions_data(){
            populateScreenMercatorData();
        }

};

QTEST_GUILESS_MAIN(tst_QGeoTiledMapScene)
#include "tst_qgeotiledmapscene.moc"
