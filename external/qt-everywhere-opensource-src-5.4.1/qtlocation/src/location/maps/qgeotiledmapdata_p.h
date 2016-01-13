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
#ifndef QGEOTILEDMAPDATA_P_H
#define QGEOTILEDMAPDATA_P_H

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

#include <QObject>
#include <QString>

#include "qgeomapdata_p.h"
#include "qgeocameradata_p.h"
#include "qgeomaptype_p.h"

#include <QtPositioning/private/qdoublevector2d_p.h>

QT_BEGIN_NAMESPACE

class QGeoTileSpec;
class QGeoTileTexture;
class QGeoTileCache;
class QGeoTiledMapDataPrivate;
class QGeoTiledMappingManagerEngine;
class QGeoTileRequestManager;

class QQuickWindow;
class QSGNode;

class QPointF;

class Q_LOCATION_EXPORT QGeoTiledMapData : public QGeoMapData
{
    Q_OBJECT
public:
    QGeoTiledMapData(QGeoTiledMappingManagerEngine *engine, QObject *parent);
    virtual ~QGeoTiledMapData();

    QGeoTileCache *tileCache();

    QSGNode *updateSceneGraph(QSGNode *, QQuickWindow *window);

    void newTileFetched(const QGeoTileSpec &spec);

    QGeoCoordinate screenPositionToCoordinate(const QDoubleVector2D &pos, bool clipToViewport = true) const;
    QDoubleVector2D coordinateToScreenPosition(const QGeoCoordinate &coordinate, bool clipToViewport = true) const;
    void prefetchTiles();

    // Alternative to exposing this is to make tileFetched a slot, but then requestManager would
    // need to be a QObject
    QGeoTileRequestManager *getRequestManager();

    virtual int mapVersion();

protected:
    void mapResized(int width, int height);
    void changeCameraData(const QGeoCameraData &oldCameraData);
    void changeActiveMapType(const QGeoMapType mapType);
    void prefetchData();

protected Q_SLOTS:
    virtual void evaluateCopyrights(const QSet<QGeoTileSpec> &visibleTiles);
    void updateMapVersion();

private:
    QGeoTiledMapDataPrivate *d_ptr;
    Q_DECLARE_PRIVATE(QGeoTiledMapData)
    Q_DISABLE_COPY(QGeoTiledMapData)

};

QT_END_NAMESPACE

#endif // QGEOMAP_P_H
