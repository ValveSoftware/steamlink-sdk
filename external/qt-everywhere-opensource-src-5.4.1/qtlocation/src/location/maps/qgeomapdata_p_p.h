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
#ifndef QGEOMAPDATA_P_P_H
#define QGEOMAPDATA_P_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QList>
#include <QSet>
#include <QVector>
#include <QPair>
#include <QPolygonF>
#include <QSizeF>
#include <QMatrix4x4>
#include <QString>

#include <QtPositioning/private/qdoublevector3d_p.h>


#include "qgeocameradata_p.h"
#include "qgeomaptype_p.h"

QT_BEGIN_NAMESPACE

class QGeoMappingManagerEngine;

class QGeoMapData;
class QGeoMapController;

class QGeoMapDataPrivate
{
public:
    QGeoMapDataPrivate(QGeoMappingManagerEngine *engine, QGeoMapData *parent);
    virtual ~QGeoMapDataPrivate();

    QGeoMappingManagerEngine *engine() const;

    QGeoMapController *mapController();

    void setCameraData(const QGeoCameraData &cameraData);
    QGeoCameraData cameraData() const;

    void resize(int width, int height);
    int width() const;
    int height() const;
    double aspectRatio() const;

    const QGeoMapType activeMapType() const;
    void setActiveMapType(const QGeoMapType &mapType);
    QString pluginString();

private:
    int width_;
    int height_;
    double aspectRatio_;

    QGeoMapData *map_;
    QGeoMappingManagerEngine *engine_;
    QString pluginString_;
    QGeoMapController *controller_;

    QGeoCameraData cameraData_;
    QGeoMapType activeMapType_;
};

QT_END_NAMESPACE

#endif // QGEOMAP_P_P_H
