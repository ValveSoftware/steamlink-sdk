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

#ifndef QDECLARATIVE3DGRAPHICSGEOMAP_H
#define QDECLARATIVE3DGRAPHICSGEOMAP_H

#include <QPointer>
#include <QTouchEvent>
#include <QtQuick/QQuickItem>
#include <QtCore/QMutex>

#include <QtCore/QCoreApplication>

#include <QtQuick/QSGTexture>
#include <QtQuick/QQuickPaintedItem>
#include <QtQml/QQmlParserStatus>
#include "qdeclarativegeomapitemview_p.h"
#include "qdeclarativegeomapgesturearea_p.h"
#include "qgeomapcontroller_p.h"
#include "qdeclarativegeomapcopyrightsnotice_p.h"

//#define QT_DECLARATIVE_LOCATION_TRACE 1

#ifdef QT_DECLARATIVE_LOCATION_TRACE
#define QLOC_TRACE0 qDebug() << __FILE__ << __FUNCTION__;
#define QLOC_TRACE1(msg1) qDebug() << __FILE__ << __FUNCTION__ << msg1;
#define QLOC_TRACE2(msg1, msg2) qDebug() << __FILE__ << __FUNCTION__ << msg1 << msg2;
#else
#define QLOC_TRACE0
#define QLOC_TRACE1(msg1)
#define QLOC_TRACE2(msg1, msg2)
#endif

#include "qgeocameradata_p.h"
#include "qgeomap_p.h"
#include "qdeclarativegeomaptype_p.h"

QT_BEGIN_NAMESPACE

class QGLSceneNode;
class QGeoTileCache;
class Tile;
class QGeoTileSpec;
class QGeoMapSphere;
class QGeoMappingManager;

class QGeoCoordinate;
class QGeoMapObject;
class QGeoMapData;
class QGeoServiceProvider;
class QDeclarativeGeoServiceProvider;
class QDeclarativeGeoMap;
class QDeclarativeGeoMapItem;
class QDeclarativeGeoMapItemBase;
class QDeclarativeGeoMapType;

class QDeclarativeGeoMap : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(QDeclarativeGeoMapGestureArea *gesture READ gesture CONSTANT)
    Q_PROPERTY(QDeclarativeGeoServiceProvider *plugin READ plugin WRITE setPlugin NOTIFY pluginChanged)
    Q_PROPERTY(qreal minimumZoomLevel READ minimumZoomLevel WRITE setMinimumZoomLevel NOTIFY minimumZoomLevelChanged)
    Q_PROPERTY(qreal maximumZoomLevel READ maximumZoomLevel WRITE setMaximumZoomLevel NOTIFY maximumZoomLevelChanged)
    Q_PROPERTY(qreal zoomLevel READ zoomLevel WRITE setZoomLevel NOTIFY zoomLevelChanged)
    Q_PROPERTY(QDeclarativeGeoMapType *activeMapType READ activeMapType WRITE setActiveMapType NOTIFY activeMapTypeChanged)
    Q_PROPERTY(QQmlListProperty<QDeclarativeGeoMapType> supportedMapTypes READ supportedMapTypes NOTIFY supportedMapTypesChanged)
    Q_PROPERTY(QGeoCoordinate center READ center WRITE setCenter NOTIFY centerChanged)
    Q_PROPERTY(QList<QObject *> mapItems READ mapItems NOTIFY mapItemsChanged)
    Q_INTERFACES(QQmlParserStatus)

public:

    explicit QDeclarativeGeoMap(QQuickItem *parent = 0);
    ~QDeclarativeGeoMap();

    // From QQmlParserStatus
    virtual void componentComplete();

    // from QQuickItem
    virtual QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *);
    virtual void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry);

    void setPlugin(QDeclarativeGeoServiceProvider *plugin);
    QDeclarativeGeoServiceProvider *plugin() const;

    void setActiveMapType(QDeclarativeGeoMapType *mapType);
    QDeclarativeGeoMapType *activeMapType() const;

    void setMinimumZoomLevel(qreal minimumZoomLevel);
    qreal minimumZoomLevel() const;

    void setMaximumZoomLevel(qreal maximumZoomLevel);
    qreal maximumZoomLevel() const;

    void setZoomLevel(qreal zoomLevel);
    qreal zoomLevel() const;

    void setCenter(const QGeoCoordinate &center);
    QGeoCoordinate center() const;

    QQmlListProperty<QDeclarativeGeoMapType> supportedMapTypes();

    Q_INVOKABLE void removeMapItem(QDeclarativeGeoMapItemBase *item);
    Q_INVOKABLE void addMapItem(QDeclarativeGeoMapItemBase *item);
    Q_INVOKABLE void clearMapItems();
    QList<QObject *> mapItems();

    Q_INVOKABLE QGeoCoordinate toCoordinate(const QPointF &screenPosition) const;
    Q_INVOKABLE QPointF toScreenPosition(const QGeoCoordinate &coordinate) const;

    // callback for map mouse areas
    bool mouseEvent(QMouseEvent *event);

    QDeclarativeGeoMapGestureArea *gesture();

    Q_INVOKABLE void fitViewportToGeoShape(const QVariant &shape);
    Q_INVOKABLE void fitViewportToMapItems();
    Q_INVOKABLE void pan(int dx, int dy);
    Q_INVOKABLE void cameraStopped(); // optional hint for prefetch

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

    void touchEvent(QTouchEvent *event);
    void wheelEvent(QWheelEvent *event);

    bool childMouseEventFilter(QQuickItem *item, QEvent *event);

Q_SIGNALS:
    void wheelAngleChanged(QPoint angleDelta);
    void pluginChanged(QDeclarativeGeoServiceProvider *plugin);
    void zoomLevelChanged(qreal zoomLevel);
    void centerChanged(const QGeoCoordinate &coordinate);
    void activeMapTypeChanged();
    void supportedMapTypesChanged();
    void minimumZoomLevelChanged();
    void maximumZoomLevelChanged();
    void mapItemsChanged();

private Q_SLOTS:
    void updateMapDisplay(const QRectF &target);
    void mappingManagerInitialized();
    void mapZoomLevelChanged(qreal zoom);
    void pluginReady();
    void onMapChildrenChanged();

private:
    void setupMapView(QDeclarativeGeoMapItemView *view);
    void populateMap();
    void fitViewportToMapItemsRefine(bool refine);

    QDeclarativeGeoServiceProvider *plugin_;
    QGeoServiceProvider *serviceProvider_;
    QGeoMappingManager *mappingManager_;

    qreal zoomLevel_;
    qreal bearing_;
    qreal tilt_;
    QGeoCoordinate center_;

    QDeclarativeGeoMapType *activeMapType_;
    QList<QDeclarativeGeoMapType *> supportedMapTypes_;
    bool componentCompleted_;
    bool mappingManagerInitialized_;
    QList<QDeclarativeGeoMapItemView *> mapViews_;

    QDeclarativeGeoMapGestureArea *gestureArea_;

    int touchTimer_;

    QGeoMap *map_;
    QPointer<QDeclarativeGeoMapCopyrightNotice> copyrightsWPtr_;

    QList<QPointer<QDeclarativeGeoMapItemBase> > mapItems_;

    QMutex updateMutex_;
    friend class QDeclarativeGeoMapItem;
    friend class QDeclarativeGeoMapItemView;
    friend class QDeclarativeGeoMapGestureArea;
    Q_DISABLE_COPY(QDeclarativeGeoMap)
};


QT_END_NAMESPACE

QML_DECLARE_TYPE(QDeclarativeGeoMap)

#endif
