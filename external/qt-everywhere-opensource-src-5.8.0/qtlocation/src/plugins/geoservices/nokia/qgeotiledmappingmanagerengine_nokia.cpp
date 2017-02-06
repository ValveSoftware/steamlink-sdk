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

#include "qgeocameracapabilities_p.h"
#include "qgeotiledmappingmanagerengine_nokia.h"
#include "qgeotiledmap_nokia.h"
#include "qgeotilefetcher_nokia.h"
#include "qgeotilespec_p.h"
#include "qgeofiletilecachenokia.h"

#include <QDebug>
#include <QDir>
#include <QVariant>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/qmath.h>
#include <QtCore/qstandardpaths.h>

QT_BEGIN_NAMESPACE

QGeoTiledMappingManagerEngineNokia::QGeoTiledMappingManagerEngineNokia(
    QGeoNetworkAccessManager *networkManager,
    const QVariantMap &parameters,
    QGeoServiceProvider::Error *error,
    QString *errorString)
    : QGeoTiledMappingManagerEngine()
{
    Q_UNUSED(error);
    Q_UNUSED(errorString);

    QGeoCameraCapabilities capabilities;

    capabilities.setMinimumZoomLevel(0.0);
    capabilities.setMaximumZoomLevel(20.0);

    setCameraCapabilities(capabilities);

    setTileSize(QSize(256, 256));

    QList<QGeoMapType> types;
    types << QGeoMapType(QGeoMapType::StreetMap, tr("Street Map"), tr("Normal map view in daylight mode"), false, false, 1);
    types << QGeoMapType(QGeoMapType::SatelliteMapDay, tr("Satellite Map"), tr("Satellite map view in daylight mode"), false, false, 2);
    types << QGeoMapType(QGeoMapType::TerrainMap, tr("Terrain Map"), tr("Terrain map view in daylight mode"), false, false, 3);
    types << QGeoMapType(QGeoMapType::HybridMap, tr("Hybrid Map"), tr("Satellite map view with streets in daylight mode"), false, false, 4);
    types << QGeoMapType(QGeoMapType::TransitMap, tr("Transit Map"), tr("Color-reduced map view with public transport scheme in daylight mode"), false, false, 5);
    types << QGeoMapType(QGeoMapType::GrayStreetMap, tr("Gray Street Map"), tr("Color-reduced map view in daylight mode"), false, false, 6);
    types << QGeoMapType(QGeoMapType::StreetMap, tr("Mobile Street Map"), tr("Mobile normal map view in daylight mode"), true, false, 7);
    types << QGeoMapType(QGeoMapType::TerrainMap, tr("Mobile Terrain Map"), tr("Mobile terrain map view in daylight mode"), true, false, 8);
    types << QGeoMapType(QGeoMapType::HybridMap, tr("Mobile Hybrid Map"), tr("Mobile satellite map view with streets in daylight mode"), true, false, 9);
    types << QGeoMapType(QGeoMapType::TransitMap, tr("Mobile Transit Map"), tr("Mobile color-reduced map view with public transport scheme in daylight mode"), true, false, 10);
    types << QGeoMapType(QGeoMapType::GrayStreetMap, tr("Mobile Gray Street Map"), tr("Mobile color-reduced map view in daylight mode"), true, false, 11);
    types << QGeoMapType(QGeoMapType::StreetMap, tr("Custom Street Map"), tr("Normal map view in daylight mode"), false, false, 12);
    types << QGeoMapType(QGeoMapType::StreetMap, tr("Night Street Map"), tr("Normal map view in night mode"), false, true, 13);
    types << QGeoMapType(QGeoMapType::StreetMap, tr("Mobile Night Street Map"), tr("Mobile normal map view in night mode"), true, true, 14);
    types << QGeoMapType(QGeoMapType::GrayStreetMap, tr("Gray Night Street Map"), tr("Color-reduced map view in night mode (especially used for background maps)"), false, true, 15);
    types << QGeoMapType(QGeoMapType::GrayStreetMap, tr("Mobile Gray Night Street Map"), tr("Mobile color-reduced map view in night mode (especially used for background maps)"), true, true, 16);
    types << QGeoMapType(QGeoMapType::PedestrianMap, tr("Pedestrian Street Map"), tr("Pedestrian map view in daylight mode"), false, false, 17);
    types << QGeoMapType(QGeoMapType::PedestrianMap, tr("Mobile Pedestrian Street Map"), tr("Mobile pedestrian map view in daylight mode for mobile usage"), true, false, 18);
    types << QGeoMapType(QGeoMapType::PedestrianMap, tr("Pedestrian Night Street Map"), tr("Pedestrian map view in night mode"), false, true, 19);
    types << QGeoMapType(QGeoMapType::PedestrianMap, tr("Mobile Pedestrian Night Street Map"), tr("Mobile pedestrian map view in night mode for mobile usage"), true, true, 20);
    types << QGeoMapType(QGeoMapType::CarNavigationMap, tr("Car Navigation Map"), tr("Normal map view in daylight mode for car navigation"), false, false, 21);
    setSupportedMapTypes(types);

    int ppi = 72;
    if (parameters.contains(QStringLiteral("here.mapping.highdpi_tiles"))) {
        const QString param = parameters.value(QStringLiteral("here.mapping.highdpi_tiles")).toString().toLower();
        if (param == "true")
            ppi = 250;
    }

    QGeoTileFetcherNokia *fetcher = new QGeoTileFetcherNokia(parameters, networkManager, this, tileSize(), ppi);
    setTileFetcher(fetcher);

    /* TILE CACHE */
    // TODO: do this in a plugin-neutral way so that other tiled map plugins
    //       don't need this boilerplate or hardcode plugin name
    if (parameters.contains(QStringLiteral("here.mapping.cache.directory"))) {
        m_cacheDirectory = parameters.value(QStringLiteral("here.mapping.cache.directory")).toString();
    } else {
        // managerName() is not yet set, we have to hardcode the plugin name below
        m_cacheDirectory = QAbstractGeoTileCache::baseLocationCacheDirectory() + QLatin1String("here");
    }

    QGeoFileTileCache *tileCache = new QGeoFileTileCacheNokia(ppi, m_cacheDirectory);

    /*
     * Disk cache setup -- defaults to ByteSize (old behavior)
     */
    if (parameters.contains(QStringLiteral("here.mapping.cache.disk.cost_strategy"))) {
        QString cacheStrategy = parameters.value(QStringLiteral("here.mapping.cache.disk.cost_strategy")).toString().toLower();
        if (cacheStrategy == QLatin1String("bytesize"))
            tileCache->setCostStrategyDisk(QGeoFileTileCache::ByteSize);
        else
            tileCache->setCostStrategyDisk(QGeoFileTileCache::Unitary);
    } else {
        tileCache->setCostStrategyDisk(QGeoFileTileCache::ByteSize);
    }
    if (parameters.contains(QStringLiteral("here.mapping.cache.disk.size"))) {
      bool ok = false;
      int cacheSize = parameters.value(QStringLiteral("here.mapping.cache.disk.size")).toString().toInt(&ok);
      if (ok)
          tileCache->setMaxDiskUsage(cacheSize);
    }

    /*
     * Memory cache setup -- defaults to ByteSize (old behavior)
     */
    if (parameters.contains(QStringLiteral("here.mapping.cache.memory.cost_strategy"))) {
        QString cacheStrategy = parameters.value(QStringLiteral("here.mapping.cache.memory.cost_strategy")).toString().toLower();
        if (cacheStrategy == QLatin1String("bytesize"))
            tileCache->setCostStrategyMemory(QGeoFileTileCache::ByteSize);
        else
            tileCache->setCostStrategyMemory(QGeoFileTileCache::Unitary);
    } else {
        tileCache->setCostStrategyMemory(QGeoFileTileCache::ByteSize);
    }
    if (parameters.contains(QStringLiteral("here.mapping.cache.memory.size"))) {
      bool ok = false;
      int cacheSize = parameters.value(QStringLiteral("here.mapping.cache.memory.size")).toString().toInt(&ok);
      if (ok)
          tileCache->setMaxMemoryUsage(cacheSize);
    }

    /*
     * Texture cache setup -- defaults to ByteSize (old behavior)
     */
    if (parameters.contains(QStringLiteral("here.mapping.cache.texture.cost_strategy"))) {
        QString cacheStrategy = parameters.value(QStringLiteral("here.mapping.cache.texture.cost_strategy")).toString().toLower();
        if (cacheStrategy == QLatin1String("bytesize"))
            tileCache->setCostStrategyTexture(QGeoFileTileCache::ByteSize);
        else
            tileCache->setCostStrategyTexture(QGeoFileTileCache::Unitary);
    } else {
        tileCache->setCostStrategyTexture(QGeoFileTileCache::ByteSize);
    }
    if (parameters.contains(QStringLiteral("here.mapping.cache.texture.size"))) {
      bool ok = false;
      int cacheSize = parameters.value(QStringLiteral("here.mapping.cache.texture.size")).toString().toInt(&ok);
      if (ok)
          tileCache->setExtraTextureUsage(cacheSize);
    }

    setTileCache(tileCache);
    populateMapSchemes();
    loadMapVersion();
    QMetaObject::invokeMethod(fetcher, "fetchCopyrightsData", Qt::QueuedConnection);
    QMetaObject::invokeMethod(fetcher, "fetchVersionData", Qt::QueuedConnection);
}

QGeoTiledMappingManagerEngineNokia::~QGeoTiledMappingManagerEngineNokia()
{
}

void QGeoTiledMappingManagerEngineNokia::populateMapSchemes()
{
    m_mapSchemes[0] = QStringLiteral("normal.day");
    m_mapSchemes[1] = QStringLiteral("normal.day");
    m_mapSchemes[2] = QStringLiteral("satellite.day");
    m_mapSchemes[3] = QStringLiteral("terrain.day");
    m_mapSchemes[4] = QStringLiteral("hybrid.day");
    m_mapSchemes[5] = QStringLiteral("normal.day.transit");
    m_mapSchemes[6] = QStringLiteral("normal.day.grey");
    m_mapSchemes[7] = QStringLiteral("normal.day.mobile");
    m_mapSchemes[8] = QStringLiteral("terrain.day.mobile");
    m_mapSchemes[9] = QStringLiteral("hybrid.day.mobile");
    m_mapSchemes[10] = QStringLiteral("normal.day.transit.mobile");
    m_mapSchemes[11] = QStringLiteral("normal.day.grey.mobile");
    m_mapSchemes[12] = QStringLiteral("normal.day.custom");
    m_mapSchemes[13] = QStringLiteral("normal.night");
    m_mapSchemes[14] = QStringLiteral("normal.night.mobile");
    m_mapSchemes[15] = QStringLiteral("normal.night.grey");
    m_mapSchemes[16] = QStringLiteral("normal.night.grey.mobile");
    m_mapSchemes[17] = QStringLiteral("pedestrian.day");
    m_mapSchemes[18] = QStringLiteral("pedestrian.day.mobile");
    m_mapSchemes[19] = QStringLiteral("pedestrian.night");
    m_mapSchemes[20] = QStringLiteral("pedestrian.night.mobile");
    m_mapSchemes[21] = QStringLiteral("carnav.day.grey");
}

QString QGeoTiledMappingManagerEngineNokia::getScheme(int mapId)
{
    return m_mapSchemes[mapId];
}

QString QGeoTiledMappingManagerEngineNokia::getBaseScheme(int mapId)
{
    QString fullScheme(m_mapSchemes[mapId]);

    return fullScheme.section(QLatin1Char('.'), 0, 0);
}

int QGeoTiledMappingManagerEngineNokia::mapVersion()
{
    return m_mapVersion.version();
}

void QGeoTiledMappingManagerEngineNokia::loadCopyrightsDescriptorsFromJson(const QByteArray &jsonData)
{
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(jsonData));
    if (doc.isNull()) {
        qDebug() << "QGeoTiledMappingManagerEngineNokia::loadCopyrightsDescriptorsFromJson() Invalid JSon document";
        return;
    }

    QJsonObject jsonObj = doc.object();

    m_copyrights.clear();
    for (auto it = jsonObj.constBegin(), end = jsonObj.constEnd(); it != end; ++it) {
        QList<CopyrightDesc> copyrightDescList;

        QJsonArray descs = it.value().toArray();
        for (int descIndex = 0; descIndex < descs.count(); descIndex++) {
            CopyrightDesc copyrightDesc;
            QJsonObject desc = descs.at(descIndex).toObject();

            copyrightDesc.minLevel = desc["minLevel"].toDouble();
            copyrightDesc.maxLevel = desc["maxLevel"].toDouble();
            copyrightDesc.label = desc["label"].toString();
            copyrightDesc.alt  = desc["alt"].toString();

            QJsonArray coordBoxes = desc["boxes"].toArray();
            for (int boxIndex = 0; boxIndex < coordBoxes.count(); boxIndex++) {
                QJsonArray box = coordBoxes[boxIndex].toArray();
                qreal top    = box[0].toDouble();
                qreal left   = box[1].toDouble();
                qreal bottom = box[2].toDouble();
                qreal right  = box[3].toDouble();
                QGeoRectangle boundingBox(QGeoCoordinate(top > bottom? top : bottom,
                                                           left),
                                            QGeoCoordinate(top > bottom? bottom : top,
                                                           right));
                copyrightDesc.boxes << boundingBox;
            }
            copyrightDescList << copyrightDesc;
        }
        m_copyrights[it.key()] = copyrightDescList;
    }
}

void QGeoTiledMappingManagerEngineNokia::parseNewVersionInfo(const QByteArray &versionData)
{
    const QString versionString = QString::fromUtf8(versionData);

    const QStringList versionLines =  versionString.split(QLatin1Char('\n'));
    QJsonObject newVersionData;
    foreach (const QString &line, versionLines) {
        const QStringList versionInfo = line.split(':');
        if (versionInfo.size() > 1) {
            const QString versionKey = versionInfo[0].trimmed();
            const QString versionValue = versionInfo[1].trimmed();
            if (!versionKey.isEmpty() && !versionValue.isEmpty()) {
                newVersionData[versionKey] = versionValue;
            }
        }
    }

    updateVersion(newVersionData);
}

void QGeoTiledMappingManagerEngineNokia::updateVersion(const QJsonObject &newVersionData) {

    if (m_mapVersion.isNewVersion(newVersionData)) {

        m_mapVersion.setVersionData(newVersionData);
        m_mapVersion.setVersion(m_mapVersion.version() + 1);

        saveMapVersion();
        setTileVersion(m_mapVersion.version());
    }
}

void QGeoTiledMappingManagerEngineNokia::saveMapVersion()
{
    QDir saveDir(m_cacheDirectory);
    QFile saveFile(saveDir.filePath(QStringLiteral("here_version")));

    if (!saveFile.open(QIODevice::WriteOnly)) {
        qWarning("Failed to write here/nokia map version.");
        return;
    }

    saveFile.write(m_mapVersion.toJson());
    saveFile.close();
}

void QGeoTiledMappingManagerEngineNokia::loadMapVersion()
{
    QDir saveDir(m_cacheDirectory);
    QFile loadFile(saveDir.filePath(QStringLiteral("here_version")));

    if (!loadFile.open(QIODevice::ReadOnly)) {
        qWarning("Failed to read here/nokia map version.");
        return;
    }

    QByteArray saveData = loadFile.readAll();
    loadFile.close();

    QJsonDocument doc(QJsonDocument::fromJson(saveData));

    QJsonObject object = doc.object();

    m_mapVersion.setVersion(object[QStringLiteral("version")].toInt());
    m_mapVersion.setVersionData(object[QStringLiteral("data")].toObject());
    setTileVersion(m_mapVersion.version());
}

QString QGeoTiledMappingManagerEngineNokia::evaluateCopyrightsText(const QGeoMapType mapType,
                                                                   const qreal zoomLevel,
                                                                   const QSet<QGeoTileSpec> &tiles)
{
    static const QChar copyrightSymbol(0x00a9);
    typedef QSet<QGeoTileSpec>::const_iterator tile_iter;
    QGeoRectangle viewport;
    double viewX0, viewY0, viewX1, viewY1;

    tile_iter tile = tiles.constBegin();
    tile_iter lastTile = tiles.constEnd();

    if (tiles.count()) {
        double divFactor = qPow(2.0, tile->zoom());
        viewX0 = viewX1 = tile->x();
        viewY0 = viewY1 = tile->y();

        // this approach establishes a geo-bounding box from passed tiles to test for intersecition
        // with copyrights boxes.
        int numTiles = 0;
        for (; tile != lastTile; ++tile) {
            if (tile->x() < viewX0)
                viewX0 = tile->x();
            if (tile->x() > viewX1)
                viewX1 = tile->x();
            if (tile->y() < viewY0)
                viewY0 = tile->y();
            if (tile->y() > viewY1)
                viewY1 = tile->y();
            numTiles++;
        }

        viewX1++;
        viewY1++;

        QDoubleVector2D pt;

        pt.setX(viewX0 / divFactor);
        pt.setY(viewY0 / divFactor);
        viewport.setTopLeft(QGeoProjection::mercatorToCoord(pt));
        pt.setX(viewX1 / divFactor);
        pt.setY(viewY1 / divFactor);
        viewport.setBottomRight(QGeoProjection::mercatorToCoord(pt));
    }

    // TODO: the following invalidation detection algorithm may be improved later.
    QList<CopyrightDesc> descriptorList = m_copyrights[ getBaseScheme(mapType.mapId()) ];
    CopyrightDesc *descriptor;
    int descIndex, boxIndex;
    QString copyrightsText;
    QSet<QString> copyrightStrings;

    for (descIndex = 0; descIndex < descriptorList.count(); descIndex++) {
        if (descriptorList[descIndex].minLevel <= zoomLevel && zoomLevel <= descriptorList[descIndex].maxLevel) {
            descriptor = &descriptorList[descIndex];

            for (boxIndex = 0; boxIndex < descriptor->boxes.count(); boxIndex++) {
                QGeoRectangle box = descriptor->boxes[boxIndex];

                if (box.intersects(viewport)) {
                    copyrightStrings.insert(descriptor->label);
                    break;
                }
            }
            if (!descriptor->boxes.count())
                copyrightStrings.insert(descriptor->label);
        }
    }

    foreach (const QString &copyrightString, copyrightStrings) {
        if (copyrightsText.length())
            copyrightsText += QLatin1Char('\n');
        copyrightsText += copyrightSymbol;
        copyrightsText += copyrightString;
    }

    return copyrightsText;
}

QGeoMap *QGeoTiledMappingManagerEngineNokia::createMap()
{
    return new QGeoTiledMapNokia(this);
}

QT_END_NAMESPACE

