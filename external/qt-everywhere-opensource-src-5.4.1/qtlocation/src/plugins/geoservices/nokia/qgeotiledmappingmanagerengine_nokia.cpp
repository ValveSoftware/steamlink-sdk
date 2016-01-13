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
** This file is part of the Nokia services plugin for the Maps and
** Navigation API.  The use of these services, whether by use of the
** plugin or by other means, is governed by the terms and conditions
** described by the file NOKIA_TERMS_AND_CONDITIONS.txt in
** this package, located in the directory containing the Nokia services
** plugin source code.
**
****************************************************************************/

#include "qgeocameracapabilities_p.h"
#include "qgeotiledmappingmanagerengine_nokia.h"
#include "qgeotiledmapdata_nokia.h"
#include "qgeotilefetcher_nokia.h"
#include "qgeotilespec_p.h"
#include "qgeotilecache_p.h"

#include <QDebug>
#include <QDir>
#include <QVariant>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/qmath.h>

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

    setTileSize(QSize(512, 512));

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

    QGeoTileFetcherNokia *fetcher = new QGeoTileFetcherNokia(parameters, networkManager, this, tileSize());
    setTileFetcher(fetcher);

    // TODO: do this in a plugin-neutral way so that other tiled map plugins
    //       don't need this boilerplate

    QString cacheDir;
    if (parameters.contains(QStringLiteral("mapping.cache.directory")))
        cacheDir = parameters.value(QStringLiteral("mapping.cache.directory")).toString();

    QGeoTileCache *tileCache = createTileCacheWithDir(cacheDir);

    if (parameters.contains(QStringLiteral("mapping.cache.disk.size"))) {
      bool ok = false;
      int cacheSize = parameters.value(QStringLiteral("mapping.cache.disk.size")).toString().toInt(&ok);
      if (ok)
          tileCache->setMaxDiskUsage(cacheSize);
    }

    if (parameters.contains(QStringLiteral("mapping.cache.memory.size"))) {
      bool ok = false;
      int cacheSize = parameters.value(QStringLiteral("mapping.cache.memory.size")).toString().toInt(&ok);
      if (ok)
          tileCache->setMaxMemoryUsage(cacheSize);
    }

    if (parameters.contains(QStringLiteral("mapping.cache.texture.size"))) {
      bool ok = false;
      int cacheSize = parameters.value(QStringLiteral("mapping.cache.texture.size")).toString().toInt(&ok);
      if (ok)
          tileCache->setExtraTextureUsage(cacheSize);
    }

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
    QStringList keys = jsonObj.keys();

    m_copyrights.clear();
    for (int keyIndex = 0; keyIndex < keys.count(); keyIndex++) {
        QList<CopyrightDesc> copyrightDescList;

        QJsonArray descs = jsonObj[ keys[ keyIndex ] ].toArray();
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
        m_copyrights[ keys[ keyIndex ] ] = copyrightDescList;
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

        emit mapVersionChanged();
    }
}

void QGeoTiledMappingManagerEngineNokia::saveMapVersion()
{
    QDir saveDir(tileCache()->directory());
    QFile saveFile(saveDir.filePath(QStringLiteral("nokia_version")));

    if (!saveFile.open(QIODevice::WriteOnly)) {
        qWarning("Failed to write nokia map version.");
        return;
    }

    saveFile.write(m_mapVersion.toJson());
    saveFile.close();
}

void QGeoTiledMappingManagerEngineNokia::loadMapVersion()
{

    QDir saveDir(tileCache()->directory());
    QFile loadFile(saveDir.filePath(QStringLiteral("nokia_version")));

    if (!loadFile.open(QIODevice::ReadOnly)) {
        qWarning("Failed to read nokia map version.");
        return;
    }

    QByteArray saveData = loadFile.readAll();
    loadFile.close();

    QJsonDocument doc(QJsonDocument::fromJson(saveData));

    QJsonObject object = doc.object();

    m_mapVersion.setVersion(object[QStringLiteral("version")].toInt());
    m_mapVersion.setVersionData(object[QStringLiteral("data")].toObject());
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

QGeoMapData *QGeoTiledMappingManagerEngineNokia::createMapData()
{
    return new QGeoTiledMapDataNokia(this);
}

QT_END_NAMESPACE

