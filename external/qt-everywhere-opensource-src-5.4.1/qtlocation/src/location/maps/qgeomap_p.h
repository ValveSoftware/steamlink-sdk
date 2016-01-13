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
#ifndef QGEOMAP_P_H
#define QGEOMAP_P_H

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

#include "qgeocameradata_p.h"
#include "qgeomaptype_p.h"
#include <QtPositioning/private/qdoublevector2d_p.h>

QT_BEGIN_NAMESPACE

class QGeoCoordinate;

class QGeoMappingManager;

class QGeoMapController;
class QGeoCameraCapabilities;

class QGeoMapData;

class QSGNode;
class QQuickWindow;

class QPointF;

class Q_LOCATION_EXPORT QGeoMap : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QGeoCameraData camera READ cameraData WRITE setCameraData NOTIFY cameraDataChanged)
    Q_PROPERTY(QGeoMapType activeMapType READ activeMapType WRITE setActiveMapType NOTIFY activeMapTypeChanged)

public:
    QGeoMap(QGeoMapData *mapData, QObject *parent = 0);
    virtual ~QGeoMap();

    QGeoMapController *mapController();

    QSGNode *updateSceneGraph(QSGNode *oldNode, QQuickWindow *window);

    void resize(int width, int height);
    int width() const;
    int height() const;

    void setCameraData(const QGeoCameraData &cameraData);
    QGeoCameraData cameraData() const;
    QGeoCameraCapabilities cameraCapabilities() const;

    QGeoCoordinate screenPositionToCoordinate(const QDoubleVector2D &pos, bool clipToViewport = true) const;
    QDoubleVector2D coordinateToScreenPosition(const QGeoCoordinate &coordinate, bool clipToViewport = true) const;

    void setActiveMapType(const QGeoMapType mapType);
    const QGeoMapType activeMapType() const;

    QString pluginString();

public Q_SLOTS:
    void update();
    void cameraStopped(); // optional hint for prefetch

Q_SIGNALS:
    void cameraDataChanged(const QGeoCameraData &cameraData);
    void updateRequired();
    void activeMapTypeChanged();
    void copyrightsChanged(const QImage &copyrightsImage, const QPoint &copyrightsPos);

private:
    QGeoMapData *mapData_;
};

QT_END_NAMESPACE

#endif // QGEOMAP_P_H
