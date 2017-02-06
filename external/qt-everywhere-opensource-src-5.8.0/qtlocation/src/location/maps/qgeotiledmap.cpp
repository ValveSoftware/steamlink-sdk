/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "qgeotiledmap_p.h"
#include "qgeotiledmap_p_p.h"

#include "qgeotiledmappingmanagerengine_p.h"
#include "qabstractgeotilecache_p.h"
#include "qgeotilespec_p.h"

#include "qgeocameratiles_p.h"
#include "qgeotilerequestmanager_p.h"
#include "qgeotiledmapscene_p.h"
#include "qgeocameracapabilities_p.h"
#include <cmath>

QT_BEGIN_NAMESPACE
#define PREFETCH_FRUSTUM_SCALE 2.0

QGeoTiledMap::QGeoTiledMap(QGeoTiledMappingManagerEngine *engine, QObject *parent)
    : QGeoMap(*new QGeoTiledMapPrivate(engine), parent)
{
    Q_D(QGeoTiledMap);

    d->m_tileRequests = new QGeoTileRequestManager(this, engine);

    QObject::connect(engine,&QGeoTiledMappingManagerEngine::tileVersionChanged,
                     this,&QGeoTiledMap::handleTileVersionChanged);
}

QGeoTiledMap::~QGeoTiledMap()
{
    Q_D(QGeoTiledMap);
    delete d->m_tileRequests;
    d->m_tileRequests = 0;

    if (!d->m_engine.isNull()) {
        QGeoTiledMappingManagerEngine *engine = qobject_cast<QGeoTiledMappingManagerEngine*>(d->m_engine);
        Q_ASSERT(engine);
        engine->releaseMap(this);
    }
}

QGeoTileRequestManager *QGeoTiledMap::requestManager()
{
    Q_D(QGeoTiledMap);
    return d->m_tileRequests;
}

void QGeoTiledMap::updateTile(const QGeoTileSpec &spec)
{
    Q_D(QGeoTiledMap);
    d->updateTile(spec);
}

void QGeoTiledMap::setPrefetchStyle(QGeoTiledMap::PrefetchStyle style)
{
    Q_D(QGeoTiledMap);
    d->m_prefetchStyle = style;
}

QAbstractGeoTileCache *QGeoTiledMap::tileCache()
{
    Q_D(QGeoTiledMap);
    return d->m_cache;
}

QSGNode *QGeoTiledMap::updateSceneGraph(QSGNode *oldNode, QQuickWindow *window)
{
    Q_D(QGeoTiledMap);
    return d->updateSceneGraph(oldNode, window);
}

void QGeoTiledMap::prefetchData()
{
    Q_D(QGeoTiledMap);
    d->prefetchTiles();
}

void QGeoTiledMap::clearData()
{
    Q_D(QGeoTiledMap);
    d->m_cache->clearAll();
    d->m_mapScene->clearTexturedTiles();
}

void QGeoTiledMap::clearScene(int mapId)
{
    Q_D(QGeoTiledMap);
    if (activeMapType().mapId() == mapId)
        d->clearScene();
}

void QGeoTiledMap::handleTileVersionChanged()
{
    Q_D(QGeoTiledMap);
    if (!d->m_engine.isNull()) {
        QGeoTiledMappingManagerEngine* engine = qobject_cast<QGeoTiledMappingManagerEngine*>(d->m_engine);
        Q_ASSERT(engine);
        d->changeTileVersion(engine->tileVersion());
    }
}

void QGeoTiledMap::evaluateCopyrights(const QSet<QGeoTileSpec> &visibleTiles)
{
    Q_UNUSED(visibleTiles);
}

QGeoCoordinate QGeoTiledMap::itemPositionToCoordinate(const QDoubleVector2D &pos, bool clipToViewport) const
{
    Q_D(const QGeoTiledMap);
    if (clipToViewport) {
        int w = viewportWidth();
        int h = viewportHeight();

        if ((pos.x() < 0) || (w < pos.x()) || (pos.y() < 0) || (h < pos.y()))
            return QGeoCoordinate();
    }

    return d->itemPositionToCoordinate(pos);
}

QDoubleVector2D QGeoTiledMap::coordinateToItemPosition(const QGeoCoordinate &coordinate, bool clipToViewport) const
{
    Q_D(const QGeoTiledMap);
    QDoubleVector2D pos = d->coordinateToItemPosition(coordinate);

    if (clipToViewport) {
        int w = viewportWidth();
        int h = viewportHeight();
        double x = pos.x();
        double y = pos.y();
        if ((x < 0.0) || (x > w) || (y < 0) || (y > h) || qIsNaN(x) || qIsNaN(y))
            return QDoubleVector2D(qQNaN(), qQNaN());
    }

    return pos;
}

// This method returns the minimum zoom level that this specific qgeomap type allows
// at a given canvas size (width,height) and for a given tile size (usually 256).
double QGeoTiledMap::minimumZoomAtViewportSize(int width, int height) const
{
    Q_D(const QGeoTiledMap);
    double maxSize = qMax(width,height);
    double numTiles = maxSize / d->m_visibleTiles->tileSize();
    return std::log(numTiles) / std::log(2.0);
}

// This method recalculates the "no-trespassing" limits for the map center.
// This has to be done when:
// 1) the map is resized, because the meters per pixel remain the same, but
//    the amount of pixels between the center and the borders changes
// 2) when the zoom level changes, because the amount of pixels between the center
//    and the borders stays the same, but the meters per pixel change
double QGeoTiledMap::maximumCenterLatitudeAtZoom(double zoomLevel) const
{
    Q_D(const QGeoTiledMap);
    double mapEdgeSize = std::pow(2.0,zoomLevel);
    mapEdgeSize *= d->m_visibleTiles->tileSize();

    // At init time weird things happen
    int clampedWindowHeight = (viewportHeight() > mapEdgeSize) ? mapEdgeSize : viewportHeight();

    // Use the window height divided by 2 as the topmost allowed center, with respect to the map size in pixels
    double mercatorTopmost = (clampedWindowHeight * 0.5) /  mapEdgeSize ;
    QGeoCoordinate topMost = QGeoProjection::mercatorToCoord(QDoubleVector2D(0.0,mercatorTopmost));

    return topMost.latitude();
}

QDoubleVector2D QGeoTiledMap::referenceCoordinateToItemPosition(const QGeoCoordinate &coordinate) const
{
    Q_D(const QGeoTiledMap);
    QDoubleVector2D point = QGeoProjection::coordToMercator(coordinate);
    return point * std::pow(2.0, d->m_cameraData.zoomLevel()) * d->m_visibleTiles->tileSize();
}

QGeoCoordinate QGeoTiledMap::referenceItemPositionToCoordinate(const QDoubleVector2D &pos) const
{
    Q_D(const QGeoTiledMap);
    QDoubleVector2D point = pos / (std::pow(2.0, d->m_cameraData.zoomLevel()) * d->m_visibleTiles->tileSize());
    return QGeoProjection::mercatorToCoord(point);
}

QGeoTiledMapPrivate::QGeoTiledMapPrivate(QGeoTiledMappingManagerEngine *engine)
    : QGeoMapPrivate(engine),
      m_cache(engine->tileCache()),
      m_visibleTiles(new QGeoCameraTiles()),
      m_prefetchTiles(new QGeoCameraTiles()),
      m_mapScene(new QGeoTiledMapScene()),
      m_tileRequests(0),
      m_maxZoomLevel(static_cast<int>(std::ceil(engine->cameraCapabilities().maximumZoomLevel()))),
      m_minZoomLevel(static_cast<int>(std::ceil(engine->cameraCapabilities().minimumZoomLevel()))),
      m_prefetchStyle(QGeoTiledMap::PrefetchTwoNeighbourLayers)
{
    int tileSize = engine->tileSize().width();
    QString pluginString(engine->managerName() + QLatin1Char('_') + QString::number(engine->managerVersion()));
    m_visibleTiles->setTileSize(tileSize);
    m_prefetchTiles->setTileSize(tileSize);
    m_visibleTiles->setPluginString(pluginString);
    m_prefetchTiles->setPluginString(pluginString);
    m_mapScene->setTileSize(tileSize);
}

QGeoTiledMapPrivate::~QGeoTiledMapPrivate()
{
    // controller_ is a child of map_, don't need to delete it here

    delete m_mapScene;
    delete m_visibleTiles;
    delete m_prefetchTiles;

    // TODO map items are not deallocated!
    // However: how to ensure this is done in rendering thread?
}

void QGeoTiledMapPrivate::prefetchTiles()
{
    if (m_tileRequests) {

        QSet<QGeoTileSpec> tiles;
        QGeoCameraData camera = m_visibleTiles->cameraData();
        int currentIntZoom = static_cast<int>(std::floor(camera.zoomLevel()));

        m_prefetchTiles->setCameraData(camera);
        m_prefetchTiles->setViewExpansion(PREFETCH_FRUSTUM_SCALE);
        tiles = m_prefetchTiles->createTiles();

        switch (m_prefetchStyle) {

        case QGeoTiledMap::PrefetchNeighbourLayer: {
            double zoomFraction = camera.zoomLevel() - currentIntZoom;
            int nearestNeighbourLayer = zoomFraction > 0.5 ? currentIntZoom + 1 : currentIntZoom - 1;
            if (nearestNeighbourLayer <= m_maxZoomLevel && nearestNeighbourLayer >= m_minZoomLevel) {
                camera.setZoomLevel(nearestNeighbourLayer);
                // Approx heuristic, keeping total # prefetched tiles roughly independent of the
                // fractional zoom level.
                double neighbourScale = (1.0 + zoomFraction)/2.0;
                m_prefetchTiles->setCameraData(camera);
                m_prefetchTiles->setViewExpansion(PREFETCH_FRUSTUM_SCALE * neighbourScale);
                tiles += m_prefetchTiles->createTiles();
            }
        }
            break;

        case QGeoTiledMap::PrefetchTwoNeighbourLayers: {
            // This is a simpler strategy, we just prefetch from layer above and below
            // for the layer below we only use half the size as this fills the screen
            if (currentIntZoom > m_minZoomLevel) {
                camera.setZoomLevel(currentIntZoom - 1);
                m_prefetchTiles->setCameraData(camera);
                m_prefetchTiles->setViewExpansion(0.5);
                tiles += m_prefetchTiles->createTiles();
            }

            if (currentIntZoom < m_maxZoomLevel) {
                camera.setZoomLevel(currentIntZoom + 1);
                m_prefetchTiles->setCameraData(camera);
                m_prefetchTiles->setViewExpansion(1.0);
                tiles += m_prefetchTiles->createTiles();
            }

        }
        }

        m_tileRequests->requestTiles(tiles - m_mapScene->texturedTiles());
    }
}

QGeoMapType QGeoTiledMapPrivate::activeMapType()
{
    return m_visibleTiles->activeMapType();
}

void QGeoTiledMapPrivate::changeCameraData(const QGeoCameraData &cameraData)
{
    Q_Q(QGeoTiledMap);

    // For zoomlevel, "snap" 0.01 either side of a whole number.
    // This is so that when we turn off bilinear scaling, we're
    // snapped to the exact pixel size of the tiles
    QGeoCameraData cam = cameraData;
    int izl = static_cast<int>(std::floor(cam.zoomLevel()));
    float delta = cam.zoomLevel() - izl;

    if (delta > 0.5) {
        izl++;
        delta -= 1.0;
    }
    if (qAbs(delta) < 0.01) {
        cam.setZoomLevel(izl);
    }

    m_visibleTiles->setCameraData(cam);
    m_mapScene->setCameraData(cam);

    updateScene();
    q->sgNodeChanged();
}

void QGeoTiledMapPrivate::updateScene()
{
    Q_Q(QGeoTiledMap);
    // detect if new tiles introduced
    const QSet<QGeoTileSpec>& tiles = m_visibleTiles->createTiles();
    bool newTilesIntroduced = !m_mapScene->visibleTiles().contains(tiles);
    m_mapScene->setVisibleTiles(tiles);

    if (newTilesIntroduced)
        q->evaluateCopyrights(tiles);

    // don't request tiles that are already built and textured
    QList<QSharedPointer<QGeoTileTexture> > cachedTiles =
            m_tileRequests->requestTiles(m_visibleTiles->createTiles() - m_mapScene->texturedTiles());

    foreach (const QSharedPointer<QGeoTileTexture> &tex, cachedTiles) {
        m_mapScene->addTile(tex->spec, tex);
    }

    if (!cachedTiles.isEmpty())
        emit q->sgNodeChanged();
}

void QGeoTiledMapPrivate::changeActiveMapType(const QGeoMapType mapType)
{
    m_visibleTiles->setMapType(mapType);
    m_prefetchTiles->setMapType(mapType);
    updateScene();
}

void QGeoTiledMapPrivate::changeTileVersion(int version)
{
    m_visibleTiles->setMapVersion(version);
    m_prefetchTiles->setMapVersion(version);
    updateScene();
}

void QGeoTiledMapPrivate::clearScene()
{
    m_mapScene->clearTexturedTiles();
    m_mapScene->setVisibleTiles(QSet<QGeoTileSpec>());
    updateScene();
}

void QGeoTiledMapPrivate::changeViewportSize(const QSize& size)
{
    Q_Q(QGeoTiledMap);

    m_visibleTiles->setScreenSize(size);
    m_prefetchTiles->setScreenSize(size);
    m_mapScene->setScreenSize(size);


    if (!size.isEmpty() && m_cache) {
        // absolute minimum size: one tile each side of display, 32-bit colour
        int texCacheSize = (size.width() + m_visibleTiles->tileSize() * 2) *
                (size.height() + m_visibleTiles->tileSize() * 2) * 4;

        // multiply by 3 so the 'recent' list in the cache is big enough for
        // an entire display of tiles
        texCacheSize *= 3;
        // TODO: move this reasoning into the tilecache

        int newSize = qMax(m_cache->minTextureUsage(), texCacheSize);
        m_cache->setMinTextureUsage(newSize);
    }

    q->evaluateCopyrights(m_visibleTiles->createTiles());
    updateScene();
}

void QGeoTiledMapPrivate::updateTile(const QGeoTileSpec &spec)
{
     Q_Q(QGeoTiledMap);
    // Only promote the texture up to GPU if it is visible
    if (m_visibleTiles->createTiles().contains(spec)){
        QSharedPointer<QGeoTileTexture> tex = m_tileRequests->tileTexture(spec);
        if (!tex.isNull()) {
            m_mapScene->addTile(spec, tex);
            emit q->sgNodeChanged();
        }
    }
}

QSGNode *QGeoTiledMapPrivate::updateSceneGraph(QSGNode *oldNode, QQuickWindow *window)
{
    return m_mapScene->updateSceneGraph(oldNode, window);
}

QGeoCoordinate QGeoTiledMapPrivate::itemPositionToCoordinate(const QDoubleVector2D &pos) const
{
    return QGeoProjection::mercatorToCoord(m_mapScene->itemPositionToMercator(pos));
}

QDoubleVector2D QGeoTiledMapPrivate::coordinateToItemPosition(const QGeoCoordinate &coordinate) const
{
    return m_mapScene->mercatorToItemPosition(QGeoProjection::coordToMercator(coordinate));
}

QT_END_NAMESPACE
