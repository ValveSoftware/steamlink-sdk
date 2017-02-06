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

#include "geomapsource.h"

#include <QUrl>

QT_BEGIN_NAMESPACE

static const QString kArcGISTileScheme(QStringLiteral("/tile/${z}/${y}/${x}"));

struct MapStyleData
{
    QString name;
    QGeoMapType::MapStyle style;
};

static const MapStyleData mapStyles[] =
{
    { QStringLiteral("StreetMap"),              QGeoMapType::StreetMap },
    { QStringLiteral("SatelliteMapDay"),        QGeoMapType::SatelliteMapDay },
    { QStringLiteral("SatelliteMapNight"),      QGeoMapType::SatelliteMapNight },
    { QStringLiteral("TerrainMap"),             QGeoMapType::TerrainMap },
    { QStringLiteral("HybridMap"),              QGeoMapType::HybridMap },
    { QStringLiteral("TransitMap"),             QGeoMapType::TransitMap },
    { QStringLiteral("GrayStreetMap"),          QGeoMapType::GrayStreetMap },
    { QStringLiteral("PedestrianMap"),          QGeoMapType::PedestrianMap },
    { QStringLiteral("CarNavigationMap"),       QGeoMapType::CarNavigationMap },
    { QStringLiteral("CustomMap"),              QGeoMapType::CustomMap }
};

GeoMapSource::GeoMapSource(QGeoMapType::MapStyle style, const QString &name,
                           const QString &description, bool mobile, bool night, int mapId,
                           const QString &url, const QString &copyright) :
    QGeoMapType(style, name, description, mobile, night, mapId),
    m_url(url), m_copyright(copyright)
{
}

QString GeoMapSource::toFormat(const QString &url)
{
    QString format = url;

    if (!format.contains(QLatin1String("${")))
        format += kArcGISTileScheme;

    format.replace(QLatin1String("${z}"), QLatin1String("%1"));
    format.replace(QLatin1String("${x}"), QLatin1String("%2"));
    format.replace(QLatin1String("${y}"), QLatin1String("%3"));
    format.replace(QLatin1String("${token}"), QLatin1String("%4"));

    return format;
}

QGeoMapType::MapStyle GeoMapSource::mapStyle(const QString &styleString)
{
    for (unsigned int i = 0; i < sizeof(mapStyles)/sizeof(MapStyle); i++) {
        const MapStyleData &mapStyle = mapStyles[i];

        if (styleString.compare(mapStyle.name, Qt::CaseInsensitive) == 0)
            return mapStyle.style;
    }

    QGeoMapType::MapStyle style = static_cast<QGeoMapType::MapStyle>(styleString.toInt());
    if (style <= QGeoMapType::NoMap)
        style = QGeoMapType::CustomMap;

    return style;
}

QT_END_NAMESPACE
