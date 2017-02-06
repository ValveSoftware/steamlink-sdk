/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qgeotiledmap_test.h"
#include "qgeotilefetcher_test.h"
#include "qgeotiledmappingmanagerengine_test.h"
#include <QtCore/QString>
#include <QtTest/QtTest>
#include <QtTest/QSignalSpy>
#include <QtLocation/QGeoServiceProvider>
#include <QtLocation/private/qgeotiledmap_p.h>
#include <QtLocation/private/qgeomappingmanager_p.h>
#include <QtLocation/private/qgeocameracapabilities_p.h>

QT_USE_NAMESPACE

Q_DECLARE_METATYPE(QGeoTiledMap::PrefetchStyle)

class FetchTileCounter: public QObject
{
    Q_OBJECT
public Q_SLOTS:
    void tileFetched(const QGeoTileSpec& spec) {
        m_tiles << spec;
    }
public:
    QSet<QGeoTileSpec> m_tiles;
};

class tst_QGeoTiledMap : public QObject
{
    Q_OBJECT

public:
    tst_QGeoTiledMap();
    ~tst_QGeoTiledMap();

private:
    void waitForFetch(int count);

private Q_SLOTS:
    void initTestCase();
    void fetchTiles();
    void fetchTiles_data();

private:
    QScopedPointer<QGeoTiledMapTest> m_map;
    QScopedPointer<FetchTileCounter> m_tilesCounter;
    QGeoTileFetcherTest *m_fetcher;

};

tst_QGeoTiledMap::tst_QGeoTiledMap():
    m_fetcher(0)
{
}

tst_QGeoTiledMap::~tst_QGeoTiledMap()
{
}

void tst_QGeoTiledMap::initTestCase()
{
    // Set custom path since CI doesn't install test plugins
#ifdef Q_OS_WIN
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath() +
                                     QStringLiteral("/../../../../plugins"));
#else
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath() +
                                     QStringLiteral("/../../../plugins"));
#endif
      QVariantMap parameters;
      parameters["tileSize"] = 16;
      parameters["maxZoomLevel"] = 8;
      parameters["finishRequestImmediately"] = true;
      QGeoServiceProvider *provider = new QGeoServiceProvider("qmlgeo.test.plugin",parameters);
      provider->setAllowExperimental(true);
      QGeoMappingManager *mappingManager = provider->mappingManager();
      QVERIFY2(provider->error() == QGeoServiceProvider::NoError, "Could not load plugin: " + provider->errorString().toLatin1());
      m_map.reset(static_cast<QGeoTiledMapTest*>(mappingManager->createMap(this)));
      QVERIFY(m_map);
      m_map->setViewportSize(QSize(16, 16));
      m_fetcher = static_cast<QGeoTileFetcherTest*>(m_map->m_engine->tileFetcher());
      m_tilesCounter.reset(new FetchTileCounter());
      connect(m_fetcher, SIGNAL(tileFetched(const QGeoTileSpec&)), m_tilesCounter.data(), SLOT(tileFetched(const QGeoTileSpec&)));
}

void tst_QGeoTiledMap::fetchTiles()
{
    QFETCH(double, zoomLevel);
    QFETCH(int, visibleCount);
    QFETCH(int, prefetchCount);
    QFETCH(QGeoTiledMap::PrefetchStyle, style);
    QFETCH(int, nearestNeighbourLayer);

    m_map->setPrefetchStyle(style);

    QGeoCameraData camera;
    camera.setCenter(QGeoProjection::mercatorToCoord(QDoubleVector2D( 0.5 ,  0.5 )));

    //prev_visible
    camera.setZoomLevel(zoomLevel-1);
    m_map->clearData();
    m_tilesCounter->m_tiles.clear();
    m_map->setCameraData(camera);
    waitForFetch(visibleCount);
    QSet<QGeoTileSpec> prev_visible = m_tilesCounter->m_tiles;

    //visible + prefetch
    camera.setZoomLevel(zoomLevel);
    m_map->clearData();
    m_tilesCounter->m_tiles.clear();
    m_map->setCameraData(camera);
    waitForFetch(visibleCount);
    QSet<QGeoTileSpec> visible = m_tilesCounter->m_tiles;
    m_map->clearData();
    m_tilesCounter->m_tiles.clear();
    m_map->prefetchData();
    waitForFetch(prefetchCount);
    QSet<QGeoTileSpec> prefetched = m_tilesCounter->m_tiles;

    //next visible
    camera.setZoomLevel(zoomLevel + 1);
    m_map->clearData();
    m_tilesCounter->m_tiles.clear();
    m_map->setCameraData(camera);
    waitForFetch(visibleCount);
    QSet<QGeoTileSpec> next_visible = m_tilesCounter->m_tiles;

    QVERIFY2(visibleCount == visible.size(), "visible count incorrect");
    QVERIFY2(prefetchCount == prefetched.size(), "prefetch count incorrect");
    QSetIterator<QGeoTileSpec> i(visible);
    while (i.hasNext())
        QVERIFY2(prefetched.contains(i.next()),"visible tile missing from prefetched tiles");

    //for zoomLevels wihtout fractions more tiles are fetched for current zoomlevel due to ViewExpansion
    if (qCeil(zoomLevel) != zoomLevel && style == QGeoTiledMap::PrefetchNeighbourLayer && nearestNeighbourLayer < zoomLevel)
        QVERIFY2(prefetched == prev_visible + visible, "wrongly prefetched tiles");

    if (qCeil(zoomLevel) != zoomLevel && style == QGeoTiledMap::PrefetchNeighbourLayer && nearestNeighbourLayer > zoomLevel)
        QVERIFY2(prefetched == next_visible + visible, "wrongly prefetched tiles");

    if (qCeil(zoomLevel) != zoomLevel && style == QGeoTiledMap::PrefetchTwoNeighbourLayers)
        QVERIFY2(prefetched == prev_visible + visible + next_visible, "wrongly prefetched tiles");
}

void tst_QGeoTiledMap::fetchTiles_data()
{
    QTest::addColumn<double>("zoomLevel");
    QTest::addColumn<int>("visibleCount");
    QTest::addColumn<int>("prefetchCount");
    QTest::addColumn<QGeoTiledMap::PrefetchStyle>("style");
    QTest::addColumn<int>("nearestNeighbourLayer");
    QTest::newRow("zoomLevel: 4 , visible count: 4 : prefetch count: 16") << 4.0 << 4 << 4 + 16 << QGeoTiledMap::PrefetchNeighbourLayer << 3;
    QTest::newRow("zoomLevel: 4.06 , visible count: 4 : prefetch count: 4") << 4.06 << 4 << 4 + 4 << QGeoTiledMap::PrefetchNeighbourLayer << 3;
    QTest::newRow("zoomLevel: 4.1 , visible count: 4 : prefetch count: 4") << 4.1 << 4 << 4 + 4 << QGeoTiledMap::PrefetchNeighbourLayer << 3;
    QTest::newRow("zoomLevel: 4.5 , visible count: 4 : prefetch count: 4") << 4.5 << 4 << 4 + 4 << QGeoTiledMap::PrefetchNeighbourLayer << 3;
    QTest::newRow("zoomLevel: 4.6 , visible count: 4 : prefetch count: 4") << 4.6 << 4 << 4 + 4 << QGeoTiledMap::PrefetchNeighbourLayer << 5;
    QTest::newRow("zoomLevel: 4.9 , visible count: 4 : prefetch count: 4") << 4.9 << 4 <<4 + 4 << QGeoTiledMap::PrefetchNeighbourLayer << 5;
    QTest::newRow("zoomLevel: 4 , visible count: 4 : prefetch count: 4") << 4.0 << 4 << 16 + 4 + 4 << QGeoTiledMap::PrefetchTwoNeighbourLayers << 3;
    QTest::newRow("zoomLevel: 4.1 , visible count: 4 : prefetch count: 4") << 4.1 << 4 << 4 + 4 + 4 << QGeoTiledMap::PrefetchTwoNeighbourLayers << 3;
    QTest::newRow("zoomLevel: 4.6 ,visible count: 4 : prefetch count: 4") << 4.6 << 4 << 4 + 4  + 4 << QGeoTiledMap::PrefetchTwoNeighbourLayers << 5;
}

void tst_QGeoTiledMap::waitForFetch(int count)
{
    int timeout = 0;
    while (m_tilesCounter->m_tiles.count() < count && timeout < count) {
        //250ms for each tile fetch
        QTest::qWait(250);
        timeout++;
    }
}

QTEST_MAIN(tst_QGeoTiledMap)

#include "tst_qgeotiledmap.moc"
