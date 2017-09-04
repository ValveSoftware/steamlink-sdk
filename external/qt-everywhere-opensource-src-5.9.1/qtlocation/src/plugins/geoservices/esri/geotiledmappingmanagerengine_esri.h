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

#ifndef GEOTILEDMAPPINGMANAGERENGINEESRI_H
#define GEOTILEDMAPPINGMANAGERENGINEESRI_H

#include <QGeoServiceProvider>

#include <QtLocation/private/qgeotiledmappingmanagerengine_p.h>

#include "geomapsource.h"

QT_BEGIN_NAMESPACE

class GeoTiledMappingManagerEngineEsri : public QGeoTiledMappingManagerEngine
{
    Q_OBJECT

public:
    GeoTiledMappingManagerEngineEsri(const QVariantMap &parameters,
                                     QGeoServiceProvider::Error *error, QString *errorString);
    virtual ~GeoTiledMappingManagerEngineEsri();

    QGeoMap *createMap() Q_DECL_OVERRIDE;

    inline const QList<GeoMapSource *>& mapSources() const;
    GeoMapSource *mapSource(int mapId) const;

private:
    bool initializeMapSources(QGeoServiceProvider::Error *error, QString *errorString);

    QList<GeoMapSource *> m_mapSources;
};

inline const QList<GeoMapSource *>& GeoTiledMappingManagerEngineEsri::mapSources() const
{
    return m_mapSources;
}

QT_END_NAMESPACE

#endif // GEOTILEDMAPPINGMANAGERENGINEESRI_H
