/****************************************************************************
**
** Copyright (C) 2016 Aaron McCarthy <mccarthy.aaron@gmail.com>
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

#include "qgeotiledmaposm.h"
#include "qgeotiledmappingmanagerengineosm.h"
#include "qgeotilefetcherosm.h"

#include <QtLocation/private/qgeotilespec_p.h>

QT_BEGIN_NAMESPACE

QGeoTiledMapOsm::QGeoTiledMapOsm(QGeoTiledMappingManagerEngineOsm *engine, QObject *parent)
:   QGeoTiledMap(engine, parent), m_mapId(-1), m_engine(engine)
{
    // Needed because evaluateCopyrights() is only triggered if visible tiles change in the map.
    // It fails the first time it gets called if providers aren't resolved, and subsequent calls
    // to it will be skipped until visible tiles change.
    // This connection makes sure the copyrights are evaluated when copyright data are ready regardless
    // of what tiles are visible.
    connect(qobject_cast<QGeoTileFetcherOsm *>(engine->tileFetcher()), &QGeoTileFetcherOsm::providerDataUpdated,
            this, &QGeoTiledMapOsm::onProviderDataUpdated);
}

QGeoTiledMapOsm::~QGeoTiledMapOsm()
{
}

void QGeoTiledMapOsm::evaluateCopyrights(const QSet<QGeoTileSpec> &visibleTiles)
{
    if (visibleTiles.isEmpty())
        return;

    QGeoTileSpec tile = *visibleTiles.constBegin();
    if (tile.mapId() == m_mapId)
        return;

    int providerId = tile.mapId() - 1;
    if (providerId < 0 || providerId >= m_engine->providers().size())
        return;

    m_mapId = tile.mapId();
    if (!m_engine->providers().at(providerId)->isValid())
        return;

    onProviderDataUpdated(m_engine->providers().at(providerId));
}

void QGeoTiledMapOsm::onProviderDataUpdated(const QGeoTileProviderOsm *provider)
{
    if (!provider->isResolved() || provider->mapType().mapId() != m_mapId)
        return;
    QString copyRights;
    const QString mapCopy = provider->mapCopyRight();
    const QString dataCopy = provider->dataCopyRight();
    const QString styleCopy = provider->styleCopyRight();
    if (!mapCopy.isEmpty()) {
        copyRights += QStringLiteral("Map &copy; ");
        copyRights += mapCopy;
    }
    if (!dataCopy.isEmpty()) {
        if (!copyRights.isEmpty())
            copyRights += QStringLiteral("<br/>");
        copyRights += QStringLiteral("Data &copy; ");
        copyRights += dataCopy;
    }
    if (!styleCopy.isEmpty()) {
        if (!copyRights.isEmpty())
            copyRights += QStringLiteral("<br/>");
        copyRights += QStringLiteral("Style &copy; ");
        copyRights += styleCopy;
    }

    if (copyRights.isEmpty() && provider->mapType().style() == QGeoMapType::CustomMap)
        copyRights = m_engine->customCopyright();

    emit copyrightsChanged(copyRights);
}

QT_END_NAMESPACE
