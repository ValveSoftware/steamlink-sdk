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
#ifndef QGEOMAPSCENE_P_H
#define QGEOMAPSCENE_P_H

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
#include <QSet>
#include <QSharedPointer>
#include <QSize>
#include <QtLocation/qlocationglobal.h>
#include <QtPositioning/private/qdoublevector2d_p.h>

QT_BEGIN_NAMESPACE

class QGeoCoordinate;
class QGeoCameraData;
class QGeoTileSpec;

class QDoubleVector2D;

class QGeoTileTexture;

class QSGNode;
class QQuickWindow;

class QPointF;

class QGeoMapScenePrivate;

class Q_LOCATION_EXPORT QGeoMapScene : public QObject
{
    Q_OBJECT
public:
    QGeoMapScene();
    virtual ~QGeoMapScene();

    void setScreenSize(const QSize &size);
    void setTileSize(int tileSize);
    void setCameraData(const QGeoCameraData &cameraData_);

    void setVisibleTiles(const QSet<QGeoTileSpec> &tiles);

    void setUseVerticalLock(bool lock);

    void addTile(const QGeoTileSpec &spec, QSharedPointer<QGeoTileTexture> texture);

    QDoubleVector2D screenPositionToMercator(const QDoubleVector2D &pos) const;
    QDoubleVector2D mercatorToScreenPosition(const QDoubleVector2D &mercator) const;

    QSGNode *updateSceneGraph(QSGNode *oldNode, QQuickWindow *window);

    bool verticalLock() const;
    QSet<QGeoTileSpec> texturedTiles();

Q_SIGNALS:
    void newTilesVisible(const QSet<QGeoTileSpec> &newTiles);

private:
    QGeoMapScenePrivate *d_ptr;
    Q_DECLARE_PRIVATE(QGeoMapScene)
    Q_DISABLE_COPY(QGeoMapScene)
};

QT_END_NAMESPACE

#endif // QGEOMAPSCENE_P_H
