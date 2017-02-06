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

#include <QtLocation/private/qgeocameradata_p.h>
#include <QtLocation/private/qgeomaptype_p.h>
#include <QtCore/QObject>
#include <QtPositioning/private/qdoublevector2d_p.h>

QT_BEGIN_NAMESPACE

class QGeoMappingManagerEngine;
class QGeoMapPrivate;
class QGeoMapController;
class QGeoCameraCapabilities;
class QGeoCoordinate;
class QSGNode;
class QQuickWindow;

class Q_LOCATION_EXPORT QGeoMap : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QGeoMap)

public:
    virtual ~QGeoMap();

    // Sets the display size
    void setViewportSize(const QSize& viewportSize);
    QSize viewportSize() const;
    int viewportWidth() const;
    int viewportHeight() const;


    QGeoCameraData cameraData() const;
    QGeoCameraCapabilities cameraCapabilities() const;

    void setActiveMapType(const QGeoMapType mapType);
    const QGeoMapType activeMapType() const;

    virtual QGeoCoordinate itemPositionToCoordinate(const QDoubleVector2D &pos, bool clipToViewport = true) const = 0;
    virtual QDoubleVector2D coordinateToItemPosition(const QGeoCoordinate &coordinate, bool clipToViewport = true) const = 0;
    virtual double minimumZoomAtViewportSize(int viewportWidth, int viewportHeight) const = 0;
    virtual double maximumCenterLatitudeAtZoom(double zoomLevel) const = 0;

    virtual QDoubleVector2D referenceCoordinateToItemPosition(const QGeoCoordinate &coordinate) const = 0;
    virtual QGeoCoordinate referenceItemPositionToCoordinate(const QDoubleVector2D &pos) const = 0;

    virtual void prefetchData();
    virtual void clearData();

protected:
    QGeoMap(QGeoMapPrivate &dd, QObject *parent = 0);
    void setCameraData(const QGeoCameraData &cameraData);
    virtual QSGNode *updateSceneGraph(QSGNode *node, QQuickWindow *window) = 0;

Q_SIGNALS:
    void cameraDataChanged(const QGeoCameraData &cameraData);
    void sgNodeChanged();
    void activeMapTypeChanged();
    void copyrightsChanged(const QImage &copyrightsImage);
    void copyrightsChanged(const QString &copyrightsHtml);

private:
    Q_DISABLE_COPY(QGeoMap)
    friend class QGeoMapController; //setCameraData
    friend class QDeclarativeGeoMap; //updateSceneGraph
};

QT_END_NAMESPACE

#endif // QGEOMAP_P_H
