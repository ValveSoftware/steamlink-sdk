/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
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
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "qgeotiledmapdata_p.h"
#include "qgeotiledmapdata_p_p.h"

#include "qgeotiledmappingmanagerengine_p.h"
#include "qgeotilecache_p.h"
#include "qgeotilespec_p.h"

#include "qgeocameratiles_p.h"
#include "qgeotilerequestmanager_p.h"
#include "qgeomapscene_p.h"
#include "qgeocameracapabilities_p.h"

#include <QMutex>
#include <QMap>

#include <qnumeric.h>

#include <QtPositioning/private/qgeoprojection_p.h>
#include <QtPositioning/private/qdoublevector2d_p.h>

#include <cmath>

QT_BEGIN_NAMESPACE


QGeoTiledMapData::QGeoTiledMapData(QGeoTiledMappingManagerEngine *engine, QObject *parent)
    : QGeoMapData(engine, parent)
{
    d_ptr = new QGeoTiledMapDataPrivate(this, engine);
    engine->registerMap(this);

    connect(engine,
            SIGNAL(mapVersionChanged()),
            this,
            SLOT(updateMapVersion()));
    QMetaObject::invokeMethod(this, "updateMapVersion", Qt::QueuedConnection);
}

QGeoTiledMapData::~QGeoTiledMapData()
{
    if (d_ptr->engine()) // check if engine hasn't already been deleted
        d_ptr->engine().data()->deregisterMap(this);
    delete d_ptr;
}
QGeoTileRequestManager *QGeoTiledMapData::getRequestManager()
{
    Q_D(QGeoTiledMapData);
    return d->tileRequests_;
}

void QGeoTiledMapData::newTileFetched(const QGeoTileSpec &spec)
{
    Q_D(QGeoTiledMapData);
    d->newTileFetched(spec);
}

QGeoTileCache *QGeoTiledMapData::tileCache()
{
    Q_D(QGeoTiledMapData);
    return d->tileCache();
}

QSGNode *QGeoTiledMapData::updateSceneGraph(QSGNode *oldNode, QQuickWindow *window)
{
    Q_D(QGeoTiledMapData);
    return d->updateSceneGraph(oldNode, window);
}

void QGeoTiledMapData::mapResized(int width, int height)
{
    Q_D(QGeoTiledMapData);
    d->resized(width, height);
    evaluateCopyrights(d->visibleTiles());
}

void QGeoTiledMapData::changeCameraData(const QGeoCameraData &oldCameraData)
{
    Q_D(QGeoTiledMapData);
    d->changeCameraData(oldCameraData);
}

void QGeoTiledMapData::prefetchData()
{
    Q_D(QGeoTiledMapData);
    d->prefetchTiles();
}

void QGeoTiledMapData::changeActiveMapType(const QGeoMapType mapType)
{
    Q_D(QGeoTiledMapData);
    d->changeActiveMapType(mapType);
}

int QGeoTiledMapData::mapVersion()
{
    return -1;
}

void QGeoTiledMapData::updateMapVersion()
{
    Q_D(QGeoTiledMapData);
    d->changeMapVersion(mapVersion());
}

void QGeoTiledMapData::evaluateCopyrights(const QSet<QGeoTileSpec> &visibleTiles)
{
    Q_UNUSED(visibleTiles);
}

QGeoCoordinate QGeoTiledMapData::screenPositionToCoordinate(const QDoubleVector2D &pos, bool clipToViewport) const
{
    Q_D(const QGeoTiledMapData);
    if (clipToViewport) {
        int w = width();
        int h = height();

        if ((pos.x() < 0) || (w < pos.x()) || (pos.y() < 0) || (h < pos.y()))
            return QGeoCoordinate();
    }

    return d->screenPositionToCoordinate(pos);
}

QDoubleVector2D QGeoTiledMapData::coordinateToScreenPosition(const QGeoCoordinate &coordinate, bool clipToViewport) const
{
    Q_D(const QGeoTiledMapData);
    QDoubleVector2D pos = d->coordinateToScreenPosition(coordinate);

    if (clipToViewport) {
        int w = width();
        int h = height();

        if ((pos.x() < 0) || (w < pos.x()) || (pos.y() < 0) || (h < pos.y()))
            return QDoubleVector2D(qQNaN(), qQNaN());
    }

    return pos;
}

QGeoTiledMapDataPrivate::QGeoTiledMapDataPrivate(QGeoTiledMapData *parent, QGeoTiledMappingManagerEngine *engine)
    : map_(parent),
      cache_(engine->tileCache()),
      engine_(engine),
      cameraTiles_(new QGeoCameraTiles()),
      mapScene_(new QGeoMapScene()),
      tileRequests_(new QGeoTileRequestManager(parent))
{
    cameraTiles_->setMaximumZoomLevel(static_cast<int>(std::ceil(engine->cameraCapabilities().maximumZoomLevel())));
    cameraTiles_->setTileSize(engine->tileSize().width());
    cameraTiles_->setPluginString(map_->pluginString());

    mapScene_->setTileSize(engine->tileSize().width());

    QObject::connect(mapScene_,
                     SIGNAL(newTilesVisible(QSet<QGeoTileSpec>)),
                     map_,
                     SLOT(evaluateCopyrights(QSet<QGeoTileSpec>)));
}

QGeoTiledMapDataPrivate::~QGeoTiledMapDataPrivate()
{
    // controller_ is a child of map_, don't need to delete it here

    delete tileRequests_;
    delete mapScene_;
    delete cameraTiles_;

    // TODO map items are not deallocated!
    // However: how to ensure this is done in rendering thread?
}

QGeoTileCache *QGeoTiledMapDataPrivate::tileCache()
{
    return cache_;
}

QPointer<QGeoTiledMappingManagerEngine> QGeoTiledMapDataPrivate::engine() const
{
    return engine_;
}

void QGeoTiledMapDataPrivate::prefetchTiles()
{
    cameraTiles_->findPrefetchTiles();

    if (tileRequests_)
        tileRequests_->requestTiles(cameraTiles_->tiles() - mapScene_->texturedTiles());
}

void QGeoTiledMapDataPrivate::changeCameraData(const QGeoCameraData &oldCameraData)
{
    double lat = oldCameraData.center().latitude();

    if (mapScene_->verticalLock()) {
        QGeoCoordinate coord = map_->cameraData().center();
        coord.setLatitude(lat);
        map_->cameraData().setCenter(coord);
    }

    // For zoomlevel, "snap" 0.05 either side of a whole number.
    // This is so that when we turn off bilinear scaling, we're
    // snapped to the exact pixel size of the tiles
    QGeoCameraData cam = map_->cameraData();
    int izl = static_cast<int>(std::floor(cam.zoomLevel()));
    float delta = cam.zoomLevel() - izl;
    if (delta > 0.5) {
        izl++;
        delta -= 1.0;
    }
    if (qAbs(delta) < 0.05) {
        cam.setZoomLevel(izl);
    }

    cameraTiles_->setCamera(cam);

    mapScene_->setCameraData(cam);
    mapScene_->setVisibleTiles(cameraTiles_->tiles());

    if (tileRequests_) {
        // don't request tiles that are already built and textured
        QList<QSharedPointer<QGeoTileTexture> > cachedTiles =
                tileRequests_->requestTiles(cameraTiles_->tiles() - mapScene_->texturedTiles());

        foreach (const QSharedPointer<QGeoTileTexture> &tex, cachedTiles) {
            mapScene_->addTile(tex->spec, tex);
        }

        if (!cachedTiles.isEmpty())
            map_->update();
    }
}

void QGeoTiledMapDataPrivate::changeActiveMapType(const QGeoMapType mapType)
{
    cameraTiles_->setMapType(mapType);
}

void QGeoTiledMapDataPrivate::changeMapVersion(int mapVersion)
{
    cameraTiles_->setMapVersion(mapVersion);
}

void QGeoTiledMapDataPrivate::resized(int width, int height)
{
    if (cameraTiles_)
        cameraTiles_->setScreenSize(QSize(width, height));
    if (mapScene_)
        mapScene_->setScreenSize(QSize(width, height));
    if (map_)
        map_->setCameraData(map_->cameraData());

    if (width > 0 && height > 0 && cache_ && cameraTiles_) {
        // absolute minimum size: one tile each side of display, 32-bit colour
        int texCacheSize = (width + cameraTiles_->tileSize() * 2) *
                (height + cameraTiles_->tileSize() * 2) * 4;

        // multiply by 3 so the 'recent' list in the cache is big enough for
        // an entire display of tiles
        texCacheSize *= 3;
        // TODO: move this reasoning into the tilecache

        int newSize = qMax(cache_->minTextureUsage(), texCacheSize);
        cache_->setMinTextureUsage(newSize);
    }
}

void QGeoTiledMapDataPrivate::newTileFetched(const QGeoTileSpec &spec)
{
    // Only promote the texture up to GPU if it is visible
    if (cameraTiles_->tiles().contains(spec)){
        QSharedPointer<QGeoTileTexture> tex = engine_.data()->getTileTexture(spec);
        if (tex) {
            mapScene_->addTile(spec, tex);
            map_->update();
        }
    }
}

QSet<QGeoTileSpec> QGeoTiledMapDataPrivate::visibleTiles()
{
    return cameraTiles_->tiles();
}

QSGNode *QGeoTiledMapDataPrivate::updateSceneGraph(QSGNode *oldNode, QQuickWindow *window)
{
    return mapScene_->updateSceneGraph(oldNode, window);
}

QGeoCoordinate QGeoTiledMapDataPrivate::screenPositionToCoordinate(const QDoubleVector2D &pos) const
{
    return QGeoProjection::mercatorToCoord(mapScene_->screenPositionToMercator(pos));
}

QDoubleVector2D QGeoTiledMapDataPrivate::coordinateToScreenPosition(const QGeoCoordinate &coordinate) const
{
    return mapScene_->mercatorToScreenPosition(QGeoProjection::coordToMercator(coordinate));
}

QT_END_NAMESPACE
