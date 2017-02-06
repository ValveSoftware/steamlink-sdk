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
#ifndef QGEOTILEDMAP_P_P_H
#define QGEOTILEDMAP_P_P_H

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

#include "qgeomap_p_p.h"
#include "qgeocameradata_p.h"
#include "qgeomaptype_p.h"
#include <QtPositioning/private/qdoublevector3d_p.h>
#include <QtPositioning/private/qdoublevector2d_p.h>
#include <QtCore/QPointer>

QT_BEGIN_NAMESPACE

class QGeoCameraTiles;
class QGeoTiledMapScene;
class QAbstractGeoTileCache;
class QGeoTiledMappingManagerEngine;
class QGeoTiledMap;
class QGeoTileRequestManager;
class QGeoTileSpec;
class QSGNode;
class QQuickWindow;

class QGeoTiledMapPrivate : public QGeoMapPrivate
{
    Q_DECLARE_PUBLIC(QGeoTiledMap)
public:
    QGeoTiledMapPrivate(QGeoTiledMappingManagerEngine *engine);
    ~QGeoTiledMapPrivate();

    QSGNode *updateSceneGraph(QSGNode *node, QQuickWindow *window);

    QGeoCoordinate itemPositionToCoordinate(const QDoubleVector2D &pos) const;
    QDoubleVector2D coordinateToItemPosition(const QGeoCoordinate &coordinate) const;

    void updateTile(const QGeoTileSpec &spec);
    void prefetchTiles();
    QGeoMapType activeMapType();

protected:
    void changeViewportSize(const QSize& size) Q_DECL_OVERRIDE;
    void changeCameraData(const QGeoCameraData &cameraData) Q_DECL_OVERRIDE;
    void changeActiveMapType(const QGeoMapType mapType) Q_DECL_OVERRIDE;
    void changeTileVersion(int version);
    void clearScene();

private:
    void updateScene();

private:
    QAbstractGeoTileCache *m_cache;
    QGeoCameraTiles *m_visibleTiles;
    QGeoCameraTiles *m_prefetchTiles;
    QGeoTiledMapScene *m_mapScene;
    QGeoTileRequestManager *m_tileRequests;
    int m_maxZoomLevel;
    int m_minZoomLevel;
    QGeoTiledMap::PrefetchStyle m_prefetchStyle;
    bool m_geomoteryUpdated;
    Q_DISABLE_COPY(QGeoTiledMapPrivate)
};

QT_END_NAMESPACE

#endif // QGEOTILEDMAP_P_P_H
