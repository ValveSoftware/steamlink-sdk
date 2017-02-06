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

#include "qgeotiledmappingmanagerengine_p.h"
#include "qgeotiledmappingmanagerengine_p_p.h"
#include "qgeotilefetcher_p.h"


#include "qgeotiledmap_p.h"
#include "qgeotilerequestmanager_p.h"
#include "qgeofiletilecache_p.h"
#include "qgeotilespec_p.h"

#include <QTimer>
#include <QLocale>
#include <QDir>
#include <QStandardPaths>

QT_BEGIN_NAMESPACE

QGeoTiledMappingManagerEngine::QGeoTiledMappingManagerEngine(QObject *parent)
    : QGeoMappingManagerEngine(parent),
      d_ptr(new QGeoTiledMappingManagerEnginePrivate)
{
}

/*!
    Destroys this mapping manager.
*/
QGeoTiledMappingManagerEngine::~QGeoTiledMappingManagerEngine()
{
    delete d_ptr;
}

/*!
    Sets the tile fetcher. Takes ownership of the QObject.
*/
void QGeoTiledMappingManagerEngine::setTileFetcher(QGeoTileFetcher *fetcher)
{
    Q_D(QGeoTiledMappingManagerEngine);

    if (d->fetcher_)
        d->fetcher_->deleteLater();
    fetcher->setParent(this);
    d->fetcher_ = fetcher;

    qRegisterMetaType<QGeoTileSpec>();

    connect(d->fetcher_,
            SIGNAL(tileFinished(QGeoTileSpec,QByteArray,QString)),
            this,
            SLOT(engineTileFinished(QGeoTileSpec,QByteArray,QString)),
            Qt::QueuedConnection);
    connect(d->fetcher_,
            SIGNAL(tileError(QGeoTileSpec,QString)),
            this,
            SLOT(engineTileError(QGeoTileSpec,QString)),
            Qt::QueuedConnection);

    engineInitialized();
}

QGeoTileFetcher *QGeoTiledMappingManagerEngine::tileFetcher()
{
    Q_D(QGeoTiledMappingManagerEngine);
    return d->fetcher_;
}

QGeoMap *QGeoTiledMappingManagerEngine::createMap()
{
    return NULL;
}

void QGeoTiledMappingManagerEngine::releaseMap(QGeoTiledMap *map)
{
    d_ptr->mapHash_.remove(map);

    QHash<QGeoTileSpec, QSet<QGeoTiledMap *> > newTileHash = d_ptr->tileHash_;
    typedef QHash<QGeoTileSpec, QSet<QGeoTiledMap *> >::const_iterator h_iter;
    h_iter hi = d_ptr->tileHash_.constBegin();
    h_iter hend = d_ptr->tileHash_.constEnd();
    for (; hi != hend; ++hi) {
        QSet<QGeoTiledMap *> maps = hi.value();
        if (maps.contains(map)) {
            maps.remove(map);
            if (maps.isEmpty())
                newTileHash.remove(hi.key());
            else
                newTileHash.insert(hi.key(), maps);
        }
    }
    d_ptr->tileHash_ = newTileHash;
}

void QGeoTiledMappingManagerEngine::updateTileRequests(QGeoTiledMap *map,
                                            const QSet<QGeoTileSpec> &tilesAdded,
                                            const QSet<QGeoTileSpec> &tilesRemoved)
{
    Q_D(QGeoTiledMappingManagerEngine);

    typedef QSet<QGeoTileSpec>::const_iterator tile_iter;

    // add and remove tiles from tileset for this map

    QSet<QGeoTileSpec> oldTiles = d->mapHash_.value(map);

    tile_iter rem = tilesRemoved.constBegin();
    tile_iter remEnd = tilesRemoved.constEnd();
    for (; rem != remEnd; ++rem) {
        oldTiles.remove(*rem);
    }

    tile_iter add = tilesAdded.constBegin();
    tile_iter addEnd = tilesAdded.constEnd();
    for (; add != addEnd; ++add) {
        oldTiles.insert(*add);
    }

    d->mapHash_.insert(map, oldTiles);

    // add and remove map from mapset for the tiles

    QSet<QGeoTileSpec> reqTiles;
    QSet<QGeoTileSpec> cancelTiles;

    rem = tilesRemoved.constBegin();
    for (; rem != remEnd; ++rem) {
        QSet<QGeoTiledMap *> mapSet = d->tileHash_.value(*rem);
        mapSet.remove(map);
        if (mapSet.isEmpty()) {
            cancelTiles.insert(*rem);
            d->tileHash_.remove(*rem);
        } else {
            d->tileHash_.insert(*rem, mapSet);
        }
    }

    add = tilesAdded.constBegin();
    for (; add != addEnd; ++add) {
        QSet<QGeoTiledMap *> mapSet = d->tileHash_.value(*add);
        if (mapSet.isEmpty()) {
            reqTiles.insert(*add);
        }
        mapSet.insert(map);
        d->tileHash_.insert(*add, mapSet);
    }

    cancelTiles -= reqTiles;

    QMetaObject::invokeMethod(d->fetcher_, "updateTileRequests",
                              Qt::QueuedConnection,
                              Q_ARG(QSet<QGeoTileSpec>, reqTiles),
                              Q_ARG(QSet<QGeoTileSpec>, cancelTiles));
}

void QGeoTiledMappingManagerEngine::engineTileFinished(const QGeoTileSpec &spec, const QByteArray &bytes, const QString &format)
{
    Q_D(QGeoTiledMappingManagerEngine);

    QSet<QGeoTiledMap *> maps = d->tileHash_.value(spec);

    typedef QSet<QGeoTiledMap *>::const_iterator map_iter;

    map_iter map = maps.constBegin();
    map_iter mapEnd = maps.constEnd();
    for (; map != mapEnd; ++map) {
        QSet<QGeoTileSpec> tileSet = d->mapHash_.value(*map);
        tileSet.remove(spec);
        if (tileSet.isEmpty())
            d->mapHash_.remove(*map);
        else
            d->mapHash_.insert(*map, tileSet);
    }

    d->tileHash_.remove(spec);
    tileCache()->insert(spec, bytes, format, d->cacheHint_);

    map = maps.constBegin();
    mapEnd = maps.constEnd();
    for (; map != mapEnd; ++map) {
        (*map)->requestManager()->tileFetched(spec);
    }
}

void QGeoTiledMappingManagerEngine::engineTileError(const QGeoTileSpec &spec, const QString &errorString)
{
    Q_D(QGeoTiledMappingManagerEngine);

    QSet<QGeoTiledMap *> maps = d->tileHash_.value(spec);
    typedef QSet<QGeoTiledMap *>::const_iterator map_iter;
    map_iter map = maps.constBegin();
    map_iter mapEnd = maps.constEnd();
    for (; map != mapEnd; ++map) {
        QSet<QGeoTileSpec> tileSet = d->mapHash_.value(*map);

        tileSet.remove(spec);
        if (tileSet.isEmpty())
            d->mapHash_.remove(*map);
        else
            d->mapHash_.insert(*map, tileSet);
    }
    d->tileHash_.remove(spec);

    for (map = maps.constBegin(); map != mapEnd; ++map) {
        (*map)->requestManager()->tileError(spec, errorString);
    }

    emit tileError(spec, errorString);
}

void QGeoTiledMappingManagerEngine::setTileSize(const QSize &tileSize)
{
    Q_D(QGeoTiledMappingManagerEngine);
    d->tileSize_ = tileSize;
}

void QGeoTiledMappingManagerEngine::setTileVersion(int version)
{
    Q_D(QGeoTiledMappingManagerEngine);
    if (d->m_tileVersion != version) {
        d->m_tileVersion = version;
        emit tileVersionChanged();
    }
}

QSize QGeoTiledMappingManagerEngine::tileSize() const
{
    Q_D(const QGeoTiledMappingManagerEngine);
    return d->tileSize_;
}

int QGeoTiledMappingManagerEngine::tileVersion() const
{
    Q_D(const QGeoTiledMappingManagerEngine);
    return d->m_tileVersion;
}

QGeoTiledMappingManagerEngine::CacheAreas QGeoTiledMappingManagerEngine::cacheHint() const
{
    Q_D(const QGeoTiledMappingManagerEngine);
    return d->cacheHint_;
}

void QGeoTiledMappingManagerEngine::setCacheHint(QGeoTiledMappingManagerEngine::CacheAreas cacheHint)
{
    Q_D(QGeoTiledMappingManagerEngine);
    d->cacheHint_ = cacheHint;
}

/*!
    Sets the tile cache. Takes ownership of the QObject.
*/
void QGeoTiledMappingManagerEngine::setTileCache(QAbstractGeoTileCache *cache)
{
    Q_D(QGeoTiledMappingManagerEngine);
    Q_ASSERT_X(!d->tileCache_, Q_FUNC_INFO, "This should be called only once");
    cache->setParent(this);
    d->tileCache_ = cache;
    d->tileCache_->init();
}

QAbstractGeoTileCache *QGeoTiledMappingManagerEngine::tileCache()
{
    Q_D(QGeoTiledMappingManagerEngine);
    if (!d->tileCache_) {
        QString cacheDirectory;
        if (!managerName().isEmpty())
            cacheDirectory = QAbstractGeoTileCache::baseLocationCacheDirectory() + managerName();
        d->tileCache_ = new QGeoFileTileCache(cacheDirectory);
        d->tileCache_->init();
    }
    return d->tileCache_;
}

QSharedPointer<QGeoTileTexture> QGeoTiledMappingManagerEngine::getTileTexture(const QGeoTileSpec &spec)
{
    return d_ptr->tileCache_->get(spec);
}

/*******************************************************************************
*******************************************************************************/

QGeoTiledMappingManagerEnginePrivate::QGeoTiledMappingManagerEnginePrivate()
:   m_tileVersion(-1),
    cacheHint_(QGeoTiledMappingManagerEngine::AllCaches),
    tileCache_(0),
    fetcher_(0)
{
}

QGeoTiledMappingManagerEnginePrivate::~QGeoTiledMappingManagerEnginePrivate()
{
}

#include "moc_qgeotiledmappingmanagerengine_p.cpp"

QT_END_NAMESPACE
