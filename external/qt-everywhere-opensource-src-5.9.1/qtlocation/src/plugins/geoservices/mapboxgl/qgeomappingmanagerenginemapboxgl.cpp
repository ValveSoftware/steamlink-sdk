/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Copyright (C) 2017 Mapbox, Inc.
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

#include "qgeomappingmanagerenginemapboxgl.h"
#include "qgeomapmapboxgl.h"

#include <QtCore/qstandardpaths.h>
#include <QtLocation/private/qabstractgeotilecache_p.h>
#include <QtLocation/private/qgeocameracapabilities_p.h>
#include <QtLocation/private/qgeomaptype_p.h>

#include <QDir>

QT_BEGIN_NAMESPACE

QGeoMappingManagerEngineMapboxGL::QGeoMappingManagerEngineMapboxGL(const QVariantMap &parameters, QGeoServiceProvider::Error *error, QString *errorString)
:   QGeoMappingManagerEngine()
{
    *error = QGeoServiceProvider::NoError;
    errorString->clear();

    QGeoCameraCapabilities cameraCaps;
    cameraCaps.setMinimumZoomLevel(0.0);
    cameraCaps.setMaximumZoomLevel(20.0);
    cameraCaps.setTileSize(512);
    cameraCaps.setSupportsBearing(true);
    cameraCaps.setSupportsTilting(true);
    cameraCaps.setMinimumTilt(0);
    cameraCaps.setMaximumTilt(60);
    cameraCaps.setMinimumFieldOfView(36.87);
    cameraCaps.setMaximumFieldOfView(36.87);
    setCameraCapabilities(cameraCaps);

    QList<QGeoMapType> mapTypes;
    int mapId = 0;
    const QByteArray pluginName = "mapboxgl";

    mapTypes << QGeoMapType(QGeoMapType::StreetMap, QStringLiteral("mapbox://styles/mapbox/streets-v10"),
            tr("Streets"), false, false, ++mapId, pluginName);
    mapTypes << QGeoMapType(QGeoMapType::StreetMap, QStringLiteral("mapbox://styles/mapbox/basic-v9"),
            tr("Basic"), false, false, ++mapId, pluginName);
    mapTypes << QGeoMapType(QGeoMapType::StreetMap, QStringLiteral("mapbox://styles/mapbox/bright-v9"),
            tr("Bright"), false, false, ++mapId, pluginName);
    mapTypes << QGeoMapType(QGeoMapType::TerrainMap, QStringLiteral("mapbox://styles/mapbox/outdoors-v10"),
            tr("Outdoors"), false, false, ++mapId, pluginName);
    mapTypes << QGeoMapType(QGeoMapType::SatelliteMapDay, QStringLiteral("mapbox://styles/mapbox/satellite-v9"),
            tr("Satellite"), false, false, ++mapId, pluginName);
    mapTypes << QGeoMapType(QGeoMapType::HybridMap, QStringLiteral("mapbox://styles/mapbox/satellite-streets-v10"),
            tr("Satellite Streets"), false, false, ++mapId, pluginName);
    mapTypes << QGeoMapType(QGeoMapType::GrayStreetMap, QStringLiteral("mapbox://styles/mapbox/light-v9"),
            tr("Light"), false, false, ++mapId, pluginName);
    mapTypes << QGeoMapType(QGeoMapType::GrayStreetMap, QStringLiteral("mapbox://styles/mapbox/dark-v9"),
            tr("Dark"), false, false, ++mapId, pluginName);
    mapTypes << QGeoMapType(QGeoMapType::TransitMap, QStringLiteral("mapbox://styles/mapbox/traffic-day-v1"),
            tr("Streets Traffic Day"), false, false, ++mapId, pluginName);
    mapTypes << QGeoMapType(QGeoMapType::TransitMap, QStringLiteral("mapbox://styles/mapbox/traffic-night-v1"),
            tr("Streets Traffic Night"), false, true, ++mapId, pluginName);

    if (parameters.contains(QStringLiteral("mapboxgl.mapping.additional_style_urls"))) {
        const QString ids = parameters.value(QStringLiteral("mapboxgl.mapping.additional_style_urls")).toString();
        const QStringList idList = ids.split(',', QString::SkipEmptyParts);

        for (auto it = idList.crbegin(), end = idList.crend(); it != end; ++it) {
            if ((*it).isEmpty())
                continue;

            mapTypes.prepend(QGeoMapType(QGeoMapType::CustomMap, *it,
                    tr("User provided style"), false, false, ++mapId, pluginName));
        }
    }

    setSupportedMapTypes(mapTypes);

    if (parameters.contains(QStringLiteral("mapboxgl.access_token"))) {
        m_settings.setAccessToken(parameters.value(QStringLiteral("mapboxgl.access_token")).toString());
    }

    bool memoryCache = false;
    if (parameters.contains(QStringLiteral("mapboxgl.mapping.cache.memory"))) {
        memoryCache = parameters.value(QStringLiteral("mapboxgl.mapping.cache.memory")).toBool();
        m_settings.setCacheDatabasePath(QStringLiteral(":memory:"));
    }

    QString cacheDirectory;
    if (parameters.contains(QStringLiteral("mapboxgl.mapping.cache.directory"))) {
        cacheDirectory = parameters.value(QStringLiteral("mapboxgl.mapping.cache.directory")).toString();
    } else {
        cacheDirectory = QAbstractGeoTileCache::baseLocationCacheDirectory() + QStringLiteral("mapboxgl/");
    }

    if (!memoryCache && QDir::root().mkpath(cacheDirectory)) {
        m_settings.setCacheDatabasePath(cacheDirectory + "/mapboxgl.db");
    }

    if (parameters.contains(QStringLiteral("mapboxgl.mapping.cache.size"))) {
        bool ok = false;
        int cacheSize = parameters.value(QStringLiteral("mapboxgl.mapping.cache.size")).toString().toInt(&ok);

        if (ok)
            m_settings.setCacheDatabaseMaximumSize(cacheSize);
    }

    if (parameters.contains(QStringLiteral("mapboxgl.mapping.use_fbo"))) {
        m_useFBO = parameters.value(QStringLiteral("mapboxgl.mapping.use_fbo")).toBool();
    }

    if (parameters.contains(QStringLiteral("mapboxgl.mapping.items.insert_before"))) {
        m_mapItemsBefore = parameters.value(QStringLiteral("mapboxgl.mapping.items.insert_before")).toString();
    }

    engineInitialized();
}

QGeoMappingManagerEngineMapboxGL::~QGeoMappingManagerEngineMapboxGL()
{
}

QGeoMap *QGeoMappingManagerEngineMapboxGL::createMap()
{
    QGeoMapMapboxGL* map = new QGeoMapMapboxGL(this, 0);
    map->setMapboxGLSettings(m_settings);
    map->setUseFBO(m_useFBO);
    map->setMapItemsBefore(m_mapItemsBefore);

    return map;
}

QT_END_NAMESPACE
