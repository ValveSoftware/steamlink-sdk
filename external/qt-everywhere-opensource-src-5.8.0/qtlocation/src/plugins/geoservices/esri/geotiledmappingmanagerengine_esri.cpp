/****************************************************************************
**
** Copyright (C) 2013-2016 Esri <contracts@esri.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "geotiledmappingmanagerengine_esri.h"
#include "geotiledmap_esri.h"
#include "geotilefetcher_esri.h"

#include <QtLocation/private/qgeocameracapabilities_p.h>
#include <QtLocation/private/qgeomaptype_p.h>
#include <QtLocation/private/qgeotiledmap_p.h>
#include <QtLocation/private/qgeofiletilecache_p.h>

#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

static void initResources()
{
    Q_INIT_RESOURCE(esri);
}

QT_BEGIN_NAMESPACE

static const QString kPrefixEsri(QStringLiteral("esri."));
static const QString kParamUserAgent(kPrefixEsri + QStringLiteral("useragent"));
static const QString kParamToken(kPrefixEsri + QStringLiteral("token"));
static const QString kPrefixMapping(kPrefixEsri + QStringLiteral("mapping."));
static const QString kParamMinimumZoomLevel(kPrefixMapping + QStringLiteral("minimumZoomLevel"));
static const QString kParamMaximumZoomLevel(kPrefixMapping + QStringLiteral("maximumZoomLevel"));

static const QString kPropMapSources(QStringLiteral("mapSources"));
static const QString kPropStyle(QStringLiteral("style"));
static const QString kPropName(QStringLiteral("name"));
static const QString kPropDescription(QStringLiteral("description"));
static const QString kPropMobile(QStringLiteral("mobile"));
static const QString kPropNight(QStringLiteral("night"));
static const QString kPropUrl(QStringLiteral("url"));
static const QString kPropMapId(QStringLiteral("mapId"));
static const QString kPropCopyright(QStringLiteral("copyrightText"));

GeoTiledMappingManagerEngineEsri::GeoTiledMappingManagerEngineEsri(const QVariantMap &parameters,
                                                                   QGeoServiceProvider::Error *error,
                                                                   QString *errorString) :
    QGeoTiledMappingManagerEngine()
{
    QGeoCameraCapabilities cameraCaps;

    double minimumZoomLevel = 0;
    double maximumZoomLevel = 19;

    if (parameters.contains(kParamMinimumZoomLevel))
        minimumZoomLevel = parameters[kParamMinimumZoomLevel].toDouble();

    if (parameters.contains(kParamMaximumZoomLevel))
        maximumZoomLevel = parameters[kParamMaximumZoomLevel].toDouble();

    cameraCaps.setMinimumZoomLevel(minimumZoomLevel);
    cameraCaps.setMaximumZoomLevel(maximumZoomLevel);

    setCameraCapabilities(cameraCaps);

    setTileSize(QSize(256, 256));

    if (!initializeMapSources(error, errorString))
        return;

    QList<QGeoMapType> mapTypes;

    foreach (GeoMapSource *mapSource, m_mapSources) {
        mapTypes << QGeoMapType(
                        mapSource->style(),
                        mapSource->name(),
                        mapSource->description(),
                        mapSource->mobile(),
                        mapSource->night(),
                        mapSource->mapId());
    }

    setSupportedMapTypes(mapTypes);

    GeoTileFetcherEsri *tileFetcher = new GeoTileFetcherEsri(this);

    if (parameters.contains(kParamUserAgent))
        tileFetcher->setUserAgent(parameters.value(kParamUserAgent).toString().toLatin1());

    if (parameters.contains(kParamToken))
        tileFetcher->setToken(parameters.value(kParamToken).toString());

    setTileFetcher(tileFetcher);

    /* TILE CACHE */
    QString cacheDirectory;
    if (parameters.contains(QStringLiteral("esri.mapping.cache.directory"))) {
        cacheDirectory = parameters.value(QStringLiteral("esri.mapping.cache.directory")).toString();
    } else {
        // managerName() is not yet set, we have to hardcode the plugin name below
        cacheDirectory = QAbstractGeoTileCache::baseLocationCacheDirectory() + QLatin1String("esri");
    }
    QGeoFileTileCache *tileCache = new QGeoFileTileCache(cacheDirectory);

    /*
     * Disk cache setup -- defaults to ByteSize (old behavior)
     */
    if (parameters.contains(QStringLiteral("esri.mapping.cache.disk.cost_strategy"))) {
        QString cacheStrategy = parameters.value(QStringLiteral("esri.mapping.cache.disk.cost_strategy")).toString().toLower();
        if (cacheStrategy == QLatin1String("bytesize"))
            tileCache->setCostStrategyDisk(QGeoFileTileCache::ByteSize);
        else
            tileCache->setCostStrategyDisk(QGeoFileTileCache::Unitary);
    } else {
        tileCache->setCostStrategyDisk(QGeoFileTileCache::ByteSize);
    }
    if (parameters.contains(QStringLiteral("esri.mapping.cache.disk.size"))) {
        bool ok = false;
        int cacheSize = parameters.value(QStringLiteral("esri.mapping.cache.disk.size")).toString().toInt(&ok);
        if (ok)
            tileCache->setMaxDiskUsage(cacheSize);
    }

    /*
     * Memory cache setup -- defaults to ByteSize (old behavior)
     */
    if (parameters.contains(QStringLiteral("esri.mapping.cache.memory.cost_strategy"))) {
        QString cacheStrategy = parameters.value(QStringLiteral("esri.mapping.cache.memory.cost_strategy")).toString().toLower();
        if (cacheStrategy == QLatin1String("bytesize"))
            tileCache->setCostStrategyMemory(QGeoFileTileCache::ByteSize);
        else
            tileCache->setCostStrategyMemory(QGeoFileTileCache::Unitary);
    } else {
        tileCache->setCostStrategyMemory(QGeoFileTileCache::ByteSize);
    }
    if (parameters.contains(QStringLiteral("esri.mapping.cache.memory.size"))) {
        bool ok = false;
        int cacheSize = parameters.value(QStringLiteral("esri.mapping.cache.memory.size")).toString().toInt(&ok);
        if (ok)
            tileCache->setMaxMemoryUsage(cacheSize);
    }

    /*
     * Texture cache setup -- defaults to ByteSize (old behavior)
     */
    if (parameters.contains(QStringLiteral("esri.mapping.cache.texture.cost_strategy"))) {
        QString cacheStrategy = parameters.value(QStringLiteral("esri.mapping.cache.texture.cost_strategy")).toString().toLower();
        if (cacheStrategy == QLatin1String("bytesize"))
            tileCache->setCostStrategyTexture(QGeoFileTileCache::ByteSize);
        else
            tileCache->setCostStrategyTexture(QGeoFileTileCache::Unitary);
    } else {
        tileCache->setCostStrategyTexture(QGeoFileTileCache::ByteSize);
    }
    if (parameters.contains(QStringLiteral("esri.mapping.cache.texture.size"))) {
        bool ok = false;
        int cacheSize = parameters.value(QStringLiteral("esri.mapping.cache.texture.size")).toString().toInt(&ok);
        if (ok)
            tileCache->setExtraTextureUsage(cacheSize);
    }


    setTileCache(tileCache);
    *error = QGeoServiceProvider::NoError;
    errorString->clear();
}

GeoTiledMappingManagerEngineEsri::~GeoTiledMappingManagerEngineEsri()
{
    qDeleteAll(m_mapSources);
}

QGeoMap *GeoTiledMappingManagerEngineEsri::createMap()
{
    return new GeoTiledMapEsri(this);
}

// ${z} = Zoom
// ${x} = X
// ${y} = Y
// ${token} = Token

// template = 'http://services.arcgisonline.com/ArcGIS/rest/services/World_Topo_Map/MapServer/tile/{{z}}/{{y}}/{{x}}.png'

bool GeoTiledMappingManagerEngineEsri::initializeMapSources(QGeoServiceProvider::Error *error,
                                                            QString *errorString)
{
    initResources();
    QFile mapsFile(":/maps.json");

    if (!mapsFile.open(QIODevice::ReadOnly)) {
        *error = QGeoServiceProvider::NotSupportedError;
        *errorString = Q_FUNC_INFO + QStringLiteral("Unable to open: ") + mapsFile.fileName();

        return false;
    }

    QByteArray mapsData = mapsFile.readAll();
    mapsFile.close();

    QJsonParseError parseError;

    QJsonDocument mapsDocument = QJsonDocument::fromJson(mapsData, &parseError);

    if (!mapsDocument.isObject()) {
        *error = QGeoServiceProvider::NotSupportedError;
        *errorString = Q_FUNC_INFO + QStringLiteral("JSON error: ") + (int)parseError.error
                + ", offset: " + parseError.offset
                + ", details: " + parseError.errorString();
        return false;
    }

    QVariantMap maps = mapsDocument.object().toVariantMap();

    QVariantList mapSources = maps["mapSources"].toList();

    foreach (QVariant mapSourceElement, mapSources) {
        QVariantMap mapSource = mapSourceElement.toMap();

        int mapId = mapSource[kPropMapId].toInt();
        if (mapId <= 0)
            mapId = m_mapSources.count() + 1;

        m_mapSources << new GeoMapSource(
                            GeoMapSource::mapStyle(mapSource[kPropStyle].toString()),
                            mapSource[kPropName].toString(),
                            mapSource[kPropDescription].toString(),
                            mapSource[kPropMobile].toBool(),
                            mapSource[kPropMapId].toBool(),
                            mapId,
                            GeoMapSource::toFormat(mapSource[kPropUrl].toString()),
                            mapSource[kPropCopyright].toString()
                            );
    }

    return true;
}

GeoMapSource *GeoTiledMappingManagerEngineEsri::mapSource(int mapId) const
{
    foreach (GeoMapSource *mapSource, mapSources()) {
        if (mapSource->mapId() == mapId)
            return mapSource;
    }

    return Q_NULLPTR;
}

QT_END_NAMESPACE
