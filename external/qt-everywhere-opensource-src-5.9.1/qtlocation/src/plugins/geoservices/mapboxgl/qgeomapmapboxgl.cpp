/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Copyright (C) 2017 Mapbox, Inc.
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

#include "qgeomapmapboxgl.h"
#include "qgeomapmapboxgl_p.h"
#include "qsgmapboxglnode.h"
#include "qmapboxglstylechange_p.h"

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFramebufferObject>
#include <QtLocation/private/qdeclarativecirclemapitem_p.h>
#include <QtLocation/private/qdeclarativegeomapitembase_p.h>
#include <QtLocation/private/qdeclarativepolygonmapitem_p.h>
#include <QtLocation/private/qdeclarativepolylinemapitem_p.h>
#include <QtLocation/private/qdeclarativerectanglemapitem_p.h>
#include <QtLocation/private/qgeomapparameter_p.h>
#include <QtLocation/private/qgeoprojection_p.h>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGImageNode>
#include <QtQuick/private/qsgtexture_p.h>

#include <QMapboxGL>

#include <cmath>

// FIXME: Expose from Mapbox GL constants
#define MBGL_TILE_SIZE 512.0

namespace {

// WARNING! The development token is subject to Mapbox Terms of Services
// and must not be used in production.
static char developmentToken[] =
    "pk.eyJ1IjoicXRzZGsiLCJhIjoiY2l5azV5MHh5MDAwdTMybzBybjUzZnhxYSJ9.9rfbeqPjX2BusLRDXHCOBA";

static const double invLog2 = 1.0 / std::log(2.0);

static double zoomLevelFrom256(double zoomLevelFor256, double tileSize)
{
    return std::log(std::pow(2.0, zoomLevelFor256) * 256.0 / tileSize) * invLog2;
}

} // namespace

QGeoMapMapboxGLPrivate::QGeoMapMapboxGLPrivate(QGeoMappingManagerEngineMapboxGL *engine)
    : QGeoMapPrivate(engine, new QGeoProjectionWebMercator)
{
}

QGeoMapMapboxGLPrivate::~QGeoMapMapboxGLPrivate()
{
}

QSGNode *QGeoMapMapboxGLPrivate::updateSceneGraph(QSGNode *node, QQuickWindow *window)
{
    Q_Q(QGeoMapMapboxGL);

    if (m_viewportSize.isEmpty()) {
        delete node;
        return 0;
    }

    QMapboxGL *map = 0;
    if (m_useFBO) {
        if (!node) {
            QSGMapboxGLTextureNode *mbglNode = new QSGMapboxGLTextureNode(m_settings, m_viewportSize, window->devicePixelRatio(), q);
            QObject::connect(mbglNode->map(), &QMapboxGL::mapChanged, q, &QGeoMapMapboxGL::onMapChanged);
            m_syncState = MapTypeSync | CameraDataSync | ViewportSync;
            node = mbglNode;
        }
        map = static_cast<QSGMapboxGLTextureNode *>(node)->map();
    } else {
        if (!node) {
            QSGMapboxGLRenderNode *mbglNode = new QSGMapboxGLRenderNode(m_settings, m_viewportSize, window->devicePixelRatio(), q);
            QObject::connect(mbglNode->map(), &QMapboxGL::mapChanged, q, &QGeoMapMapboxGL::onMapChanged);
            m_syncState = MapTypeSync | CameraDataSync | ViewportSync;
            node = mbglNode;
        }
        map = static_cast<QSGMapboxGLRenderNode *>(node)->map();
    }

    if (m_syncState & MapTypeSync) {
        m_developmentMode = m_activeMapType.name().startsWith("mapbox://")
            && m_settings.accessToken() == developmentToken;

        map->setStyleUrl(m_activeMapType.name());
    }

    if (m_syncState & CameraDataSync) {
        map->setZoom(zoomLevelFrom256(m_cameraData.zoomLevel() , MBGL_TILE_SIZE));
        map->setBearing(m_cameraData.bearing());
        map->setPitch(m_cameraData.tilt());

        QGeoCoordinate coordinate = m_cameraData.center();
        map->setCoordinate(QMapbox::Coordinate(coordinate.latitude(), coordinate.longitude()));
    }

    if (m_syncState & ViewportSync) {
        if (m_useFBO) {
            static_cast<QSGMapboxGLTextureNode *>(node)->resize(m_viewportSize, window->devicePixelRatio());
        } else {
            map->resize(m_viewportSize, m_viewportSize * window->devicePixelRatio());
        }
    }

    if (m_styleLoaded) {
        syncStyleChanges(map);
    }

    if (m_useFBO) {
        static_cast<QSGMapboxGLTextureNode *>(node)->render(window);
    }

    threadedRenderingHack(window, map);

    m_syncState = NoSync;

    return node;
}

void QGeoMapMapboxGLPrivate::addParameter(QGeoMapParameter *param)
{
    Q_Q(QGeoMapMapboxGL);

    QObject::connect(param, &QGeoMapParameter::propertyUpdated, q,
        &QGeoMapMapboxGL::onParameterPropertyUpdated);
}

void QGeoMapMapboxGLPrivate::removeParameter(QGeoMapParameter *param)
{
    Q_Q(QGeoMapMapboxGL);

    q->disconnect(param);
}

QGeoMap::ItemTypes QGeoMapMapboxGLPrivate::supportedMapItemTypes() const
{
    // TODO https://bugreports.qt.io/browse/QTBUG-58869
    return QGeoMap::MapRectangle | QGeoMap::MapPolygon | QGeoMap::MapPolyline;
}

void QGeoMapMapboxGLPrivate::addMapItem(QDeclarativeGeoMapItemBase *item)
{
    Q_Q(QGeoMapMapboxGL);

    switch (item->itemType()) {
    case QGeoMap::NoItem:
    case QGeoMap::MapQuickItem:
    case QGeoMap::CustomMapItem:
    case QGeoMap::MapCircle:
        return;
    case QGeoMap::MapRectangle: {
        QDeclarativeRectangleMapItem *mapItem = qobject_cast<QDeclarativeRectangleMapItem *>(item);
        QObject::connect(mapItem, &QDeclarativeRectangleMapItem::bottomRightChanged, q, &QGeoMapMapboxGL::onMapItemGeometryChanged);
        QObject::connect(mapItem, &QDeclarativeRectangleMapItem::topLeftChanged, q, &QGeoMapMapboxGL::onMapItemGeometryChanged);
        QObject::connect(mapItem, &QDeclarativeRectangleMapItem::colorChanged, q, &QGeoMapMapboxGL::onMapItemPropertyChanged);
        QObject::connect(mapItem->border(), &QDeclarativeMapLineProperties::colorChanged, q, &QGeoMapMapboxGL::onMapItemSubPropertyChanged);
        QObject::connect(mapItem->border(), &QDeclarativeMapLineProperties::widthChanged, q, &QGeoMapMapboxGL::onMapItemUnsupportedPropertyChanged);
    } break;
    case QGeoMap::MapPolygon: {
        QDeclarativePolygonMapItem *mapItem = qobject_cast<QDeclarativePolygonMapItem *>(item);
        QObject::connect(mapItem, &QDeclarativePolygonMapItem::pathChanged, q, &QGeoMapMapboxGL::onMapItemGeometryChanged);
        QObject::connect(mapItem, &QDeclarativePolygonMapItem::colorChanged, q, &QGeoMapMapboxGL::onMapItemGeometryChanged);
        QObject::connect(mapItem->border(), &QDeclarativeMapLineProperties::colorChanged, q, &QGeoMapMapboxGL::onMapItemSubPropertyChanged);
        QObject::connect(mapItem->border(), &QDeclarativeMapLineProperties::widthChanged, q, &QGeoMapMapboxGL::onMapItemUnsupportedPropertyChanged);
    } break;
    case QGeoMap::MapPolyline: {
        QDeclarativePolylineMapItem *mapItem = qobject_cast<QDeclarativePolylineMapItem *>(item);
        QObject::connect(mapItem, &QDeclarativePolylineMapItem::pathChanged, q, &QGeoMapMapboxGL::onMapItemGeometryChanged);
        QObject::connect(mapItem->line(), &QDeclarativeMapLineProperties::colorChanged, q, &QGeoMapMapboxGL::onMapItemSubPropertyChanged);
        QObject::connect(mapItem->line(), &QDeclarativeMapLineProperties::widthChanged, q, &QGeoMapMapboxGL::onMapItemSubPropertyChanged);
    } break;
    }

    QObject::connect(item, &QDeclarativeGeoMapItemBase::mapItemOpacityChanged, q, &QGeoMapMapboxGL::onMapItemPropertyChanged);

    m_styleChanges << QMapboxGLStyleChange::addMapItem(item, m_mapItemsBefore);

    emit q->sgNodeChanged();
}

void QGeoMapMapboxGLPrivate::removeMapItem(QDeclarativeGeoMapItemBase *item)
{
    Q_Q(QGeoMapMapboxGL);

    switch (item->itemType()) {
    case QGeoMap::NoItem:
    case QGeoMap::MapQuickItem:
    case QGeoMap::CustomMapItem:
    case QGeoMap::MapCircle:
        return;
    case QGeoMap::MapRectangle:
        q->disconnect(static_cast<QDeclarativeRectangleMapItem *>(item)->border());
        break;
    case QGeoMap::MapPolygon:
        q->disconnect(static_cast<QDeclarativePolygonMapItem *>(item)->border());
        break;
    case QGeoMap::MapPolyline:
        q->disconnect(static_cast<QDeclarativePolylineMapItem *>(item)->line());
        break;
    }

    q->disconnect(item);

    m_styleChanges << QMapboxGLStyleChange::removeMapItem(item);

    emit q->sgNodeChanged();
}

void QGeoMapMapboxGLPrivate::changeViewportSize(const QSize &)
{
    Q_Q(QGeoMapMapboxGL);

    m_syncState = m_syncState | ViewportSync;
    emit q->sgNodeChanged();
}

void QGeoMapMapboxGLPrivate::changeCameraData(const QGeoCameraData &)
{
    Q_Q(QGeoMapMapboxGL);

    m_syncState = m_syncState | CameraDataSync;
    emit q->sgNodeChanged();
}

void QGeoMapMapboxGLPrivate::changeActiveMapType(const QGeoMapType)
{
    Q_Q(QGeoMapMapboxGL);

    m_syncState = m_syncState | MapTypeSync;
    emit q->sgNodeChanged();
}

void QGeoMapMapboxGLPrivate::syncStyleChanges(QMapboxGL *map)
{
    for (const auto& change : m_styleChanges) {
        change->apply(map);
    }

    m_styleChanges.clear();
}

void QGeoMapMapboxGLPrivate::threadedRenderingHack(QQuickWindow *window, QMapboxGL *map)
{
    // FIXME: Optimal support for threaded rendering needs core changes
    // in Mapbox GL Native. Meanwhile we need to set a timer to update
    // the map until all the resources are loaded, which is not exactly
    // battery friendly, because might trigger more paints than we need.
    if (!m_warned) {
        m_threadedRendering = window->openglContext()->thread() != QCoreApplication::instance()->thread();

        if (m_threadedRendering) {
            qWarning() << "Threaded rendering is not optimal in the Mapbox GL plugin.";
        }

        m_warned = true;
    }

    if (m_threadedRendering) {
        if (!map->isFullyLoaded()) {
            QMetaObject::invokeMethod(&m_refresh, "start", Qt::QueuedConnection);
        } else {
            QMetaObject::invokeMethod(&m_refresh, "stop", Qt::QueuedConnection);
        }
    }
}

/*
 * QGeoMapMapboxGL implementation
 */

QGeoMapMapboxGL::QGeoMapMapboxGL(QGeoMappingManagerEngineMapboxGL *engine, QObject *parent)
    :   QGeoMap(*new QGeoMapMapboxGLPrivate(engine), parent), m_engine(engine)
{
    Q_D(QGeoMapMapboxGL);

    connect(&d->m_refresh, &QTimer::timeout, this, &QGeoMap::sgNodeChanged);
    d->m_refresh.setInterval(250);
}

QGeoMapMapboxGL::~QGeoMapMapboxGL()
{
}

QString QGeoMapMapboxGL::copyrightsStyleSheet() const
{
    return QStringLiteral("* { vertical-align: middle; font-weight: normal }");
}

void QGeoMapMapboxGL::setMapboxGLSettings(const QMapboxGLSettings& settings)
{
    Q_D(QGeoMapMapboxGL);

    d->m_settings = settings;

    // If the access token is not set, use the development access token.
    // This will only affect mapbox:// styles.
    if (d->m_settings.accessToken().isEmpty()) {
        d->m_settings.setAccessToken(developmentToken);
    }
}

void QGeoMapMapboxGL::setUseFBO(bool useFBO)
{
    Q_D(QGeoMapMapboxGL);
    d->m_useFBO = useFBO;
}

void QGeoMapMapboxGL::setMapItemsBefore(const QString &before)
{
    Q_D(QGeoMapMapboxGL);
    d->m_mapItemsBefore = before;
}

QSGNode *QGeoMapMapboxGL::updateSceneGraph(QSGNode *oldNode, QQuickWindow *window)
{
    Q_D(QGeoMapMapboxGL);
    return d->updateSceneGraph(oldNode, window);
}

void QGeoMapMapboxGL::onMapChanged(QMapboxGL::MapChange change)
{
    Q_D(QGeoMapMapboxGL);

    if (change == QMapboxGL::MapChangeDidFinishLoadingStyle || change == QMapboxGL::MapChangeDidFailLoadingMap) {
        d->m_styleLoaded = true;
    } else if (change == QMapboxGL::MapChangeWillStartLoadingMap) {
        d->m_styleLoaded = false;
        d->m_styleChanges.clear();

        for (QDeclarativeGeoMapItemBase *item : d->m_mapItems)
            d->m_styleChanges << QMapboxGLStyleChange::addMapItem(item, d->m_mapItemsBefore);

        for (QGeoMapParameter *param : d->m_mapParameters)
            d->m_styleChanges << QMapboxGLStyleChange::addMapParameter(param);
    }
}

void QGeoMapMapboxGL::onMapItemPropertyChanged()
{
    Q_D(QGeoMapMapboxGL);

    QDeclarativeGeoMapItemBase *item = static_cast<QDeclarativeGeoMapItemBase *>(sender());
    d->m_styleChanges << QMapboxGLStyleSetPaintProperty::fromMapItem(item);

    emit sgNodeChanged();
}

void QGeoMapMapboxGL::onMapItemSubPropertyChanged()
{
    Q_D(QGeoMapMapboxGL);

    QDeclarativeGeoMapItemBase *item = static_cast<QDeclarativeGeoMapItemBase *>(sender()->parent());
    d->m_styleChanges << QMapboxGLStyleSetPaintProperty::fromMapItem(item);

    emit sgNodeChanged();
}

void QGeoMapMapboxGL::onMapItemUnsupportedPropertyChanged()
{
    // TODO https://bugreports.qt.io/browse/QTBUG-58872
    qWarning() << "Unsupported property for managed Map item";
}

void QGeoMapMapboxGL::onMapItemGeometryChanged()
{
    Q_D(QGeoMapMapboxGL);

    QDeclarativeGeoMapItemBase *item = static_cast<QDeclarativeGeoMapItemBase *>(sender());
    d->m_styleChanges << QMapboxGLStyleAddSource::fromMapItem(item);

    emit sgNodeChanged();
}

void QGeoMapMapboxGL::onParameterPropertyUpdated(QGeoMapParameter *param, const char *)
{
    Q_D(QGeoMapMapboxGL);

    d->m_styleChanges.append(QMapboxGLStyleChange::addMapParameter(param));

    emit sgNodeChanged();
}

void QGeoMapMapboxGL::copyrightsChanged(const QString &copyrightsHtml)
{
    Q_D(QGeoMapMapboxGL);

    QString copyrightsHtmlFinal = copyrightsHtml;

    if (d->m_developmentMode) {
        copyrightsHtmlFinal.prepend("<a href='https://www.mapbox.com/pricing'>"
            + tr("Development access token, do not use in production!") + "</a> - ");
    }

    if (d->m_activeMapType.name().startsWith("mapbox://")) {
        copyrightsHtmlFinal = "<table><tr><th><img src='qrc:/mapboxgl/logo.png'/></th><th>"
            + copyrightsHtmlFinal + "</th></tr></table>";
    }

    QGeoMap::copyrightsChanged(copyrightsHtmlFinal);
}
