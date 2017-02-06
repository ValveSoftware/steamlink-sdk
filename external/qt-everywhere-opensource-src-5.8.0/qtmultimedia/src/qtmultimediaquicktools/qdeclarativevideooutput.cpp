/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Research In Motion
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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
#include "qdeclarativevideooutput_p.h"

#include "qdeclarativevideooutput_render_p.h"
#include "qdeclarativevideooutput_window_p.h"
#include <private/qvideooutputorientationhandler_p.h>
#include <QtMultimedia/qmediaobject.h>
#include <QtMultimedia/qmediaservice.h>
#include <private/qmediapluginloader_p.h>
#include <QtCore/qloggingcategory.h>

static void initResource() {
    Q_INIT_RESOURCE(qtmultimediaquicktools);
}

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(qLcVideo, "qt.multimedia.video")

/*!
    \qmltype VideoOutput
    \instantiates QDeclarativeVideoOutput
    \brief Render video or camera viewfinder.

    \ingroup multimedia_qml
    \ingroup multimedia_video_qml
    \inqmlmodule QtMultimedia

    \qml

    Rectangle {
        width: 800
        height: 600
        color: "black"

        MediaPlayer {
            id: player
            source: "file://video.webm"
            autoPlay: true
        }

        VideoOutput {
            id: videoOutput
            source: player
            anchors.fill: parent
        }
    }

    \endqml

    The VideoOutput item supports untransformed, stretched, and uniformly scaled video presentation.
    For a description of stretched uniformly scaled presentation, see the \l fillMode property
    description.

    The VideoOutput item works with backends that support either QVideoRendererControl or
    QVideoWindowControl. If the backend only supports QVideoWindowControl, the video is rendered
    onto an overlay window that is layered on top of the QtQuick window. Due to the nature of the
    video overlays, certain features are not available for these kind of backends:
    \list
    \li Some transformations like rotations
    \li Having other QtQuick items on top of the VideoOutput item
    \endlist
    Most backends however do support QVideoRendererControl and therefore don't have the limitations
    listed above.

    \sa MediaPlayer, Camera

\omit
    \section1 Screen Saver

    If it is likely that an application will be playing video for an extended
    period of time without user interaction it may be necessary to disable
    the platform's screen saver. The \l ScreenSaver (from \l QtSystemInfo)
    may be used to disable the screensaver in this fashion:

    \qml
    import QtSystemInfo 5.0

    ScreenSaver { screenSaverEnabled: false }
    \endqml
\endomit
*/

// TODO: Restore Qt System Info docs when the module is released

/*!
    \internal
    \class QDeclarativeVideoOutput
    \brief The QDeclarativeVideoOutput class provides a video output item.
*/

QDeclarativeVideoOutput::QDeclarativeVideoOutput(QQuickItem *parent) :
    QQuickItem(parent),
    m_sourceType(NoSource),
    m_fillMode(PreserveAspectFit),
    m_geometryDirty(true),
    m_orientation(0),
    m_autoOrientation(false),
    m_screenOrientationHandler(0)
{
    initResource();
    setFlag(ItemHasContents, true);
}

QDeclarativeVideoOutput::~QDeclarativeVideoOutput()
{
    m_backend.reset();
    m_source.clear();
    _q_updateMediaObject();
}

/*!
    \qmlproperty variant QtMultimedia::VideoOutput::source

    This property holds the source item providing the video frames like MediaPlayer or Camera.

    If you are extending your own C++ classes to interoperate with VideoOutput, you can
    either provide a QObject based class with a \c mediaObject property that exposes a
    QMediaObject derived class that has a QVideoRendererControl available, or you can
    provide a QObject based class with a writable \c videoSurface property that can
    accept a QAbstractVideoSurface based class and can follow the correct protocol to
    deliver QVideoFrames to it.
*/

void QDeclarativeVideoOutput::setSource(QObject *source)
{
    qCDebug(qLcVideo) << "source is" << source;

    if (source == m_source.data())
        return;

    if (m_source && m_sourceType == MediaObjectSource) {
        disconnect(m_source.data(), 0, this, SLOT(_q_updateMediaObject()));
        disconnect(m_source.data(), 0, this, SLOT(_q_updateCameraInfo()));
    }

    if (m_backend)
        m_backend->releaseSource();

    m_source = source;

    if (m_source) {
        const QMetaObject *metaObject = m_source.data()->metaObject();

        int mediaObjectPropertyIndex = metaObject->indexOfProperty("mediaObject");
        if (mediaObjectPropertyIndex != -1) {
            const QMetaProperty mediaObjectProperty = metaObject->property(mediaObjectPropertyIndex);

            if (mediaObjectProperty.hasNotifySignal()) {
                QMetaMethod method = mediaObjectProperty.notifySignal();
                QMetaObject::connect(m_source.data(), method.methodIndex(),
                                     this, this->metaObject()->indexOfSlot("_q_updateMediaObject()"),
                                     Qt::DirectConnection, 0);

            }

            int deviceIdPropertyIndex = metaObject->indexOfProperty("deviceId");
            if (deviceIdPropertyIndex != -1) { // Camera source
                const QMetaProperty deviceIdProperty = metaObject->property(deviceIdPropertyIndex);

                if (deviceIdProperty.hasNotifySignal()) {
                    QMetaMethod method = deviceIdProperty.notifySignal();
                    QMetaObject::connect(m_source.data(), method.methodIndex(),
                                         this, this->metaObject()->indexOfSlot("_q_updateCameraInfo()"),
                                         Qt::DirectConnection, 0);

                }
            }

            m_sourceType = MediaObjectSource;
        } else if (metaObject->indexOfProperty("videoSurface") != -1) {
            // Make sure our backend is a QDeclarativeVideoRendererBackend
            m_backend.reset();
            createBackend(0);
            Q_ASSERT(m_backend);
#ifndef QT_NO_DYNAMIC_CAST
            Q_ASSERT(dynamic_cast<QDeclarativeVideoRendererBackend *>(m_backend.data()));
#endif
            QAbstractVideoSurface * const surface = m_backend->videoSurface();
            Q_ASSERT(surface);
            m_source.data()->setProperty("videoSurface",
                                         QVariant::fromValue<QAbstractVideoSurface*>(surface));
            m_sourceType = VideoSurfaceSource;
        } else {
            m_sourceType = NoSource;
        }
    } else {
        m_sourceType = NoSource;
    }

    _q_updateMediaObject();
    emit sourceChanged();
}

Q_GLOBAL_STATIC_WITH_ARGS(QMediaPluginLoader, videoBackendFactoryLoader,
        (QDeclarativeVideoBackendFactoryInterface_iid, QLatin1String("video/declarativevideobackend"), Qt::CaseInsensitive))

bool QDeclarativeVideoOutput::createBackend(QMediaService *service)
{
    bool backendAvailable = false;

    const auto instances = videoBackendFactoryLoader()->instances(QLatin1String("declarativevideobackend"));
    for (QObject *instance : instances) {
        if (QDeclarativeVideoBackendFactoryInterface *plugin = qobject_cast<QDeclarativeVideoBackendFactoryInterface*>(instance)) {
            m_backend.reset(plugin->create(this));
            if (m_backend && m_backend->init(service)) {
                backendAvailable = true;
                break;
            }
        }
    }

    if (!backendAvailable) {
        m_backend.reset(new QDeclarativeVideoRendererBackend(this));
        if (m_backend->init(service))
            backendAvailable = true;
    }

    // QDeclarativeVideoWindowBackend only works when there is a service with a QVideoWindowControl.
    // Without service, the QDeclarativeVideoRendererBackend should always work.
    if (!backendAvailable) {
        Q_ASSERT(service);
        m_backend.reset(new QDeclarativeVideoWindowBackend(this));
        if (m_backend->init(service))
            backendAvailable = true;
    }

    if (!backendAvailable) {
        qWarning() << Q_FUNC_INFO << "Media service has neither renderer nor window control available.";
        m_backend.reset();
    } else if (!m_geometryDirty) {
        m_backend->updateGeometry();
    }

    if (m_backend) {
        m_backend->clearFilters();
        for (int i = 0; i < m_filters.count(); ++i)
            m_backend->appendFilter(m_filters[i]);
    }

    return backendAvailable;
}

void QDeclarativeVideoOutput::_q_updateMediaObject()
{
    QMediaObject *mediaObject = 0;

    if (m_source)
        mediaObject = qobject_cast<QMediaObject*>(m_source.data()->property("mediaObject").value<QObject*>());

    qCDebug(qLcVideo) << "media object is" << mediaObject;

    if (m_mediaObject.data() == mediaObject)
        return;

    if (m_sourceType != VideoSurfaceSource)
        m_backend.reset();

    m_mediaObject.clear();
    m_service.clear();

    if (mediaObject) {
        if (QMediaService *service = mediaObject->service()) {
            if (createBackend(service)) {
                m_service = service;
                m_mediaObject = mediaObject;
            }
        }
    }

    _q_updateCameraInfo();
}

void QDeclarativeVideoOutput::_q_updateCameraInfo()
{
    if (m_mediaObject) {
        const QCamera *camera = qobject_cast<const QCamera *>(m_mediaObject);
        if (camera) {
            QCameraInfo info(*camera);

            if (m_cameraInfo != info) {
                m_cameraInfo = info;

                // The camera position and orientation need to be taken into account for
                // the viewport auto orientation
                if (m_autoOrientation)
                    _q_screenOrientationChanged(m_screenOrientationHandler->currentOrientation());
            }
        }
    } else {
        m_cameraInfo = QCameraInfo();
    }
}

/*!
    \qmlproperty enumeration QtMultimedia::VideoOutput::fillMode

    Set this property to define how the video is scaled to fit the target area.

    \list
    \li Stretch - the video is scaled to fit.
    \li PreserveAspectFit - the video is scaled uniformly to fit without cropping
    \li PreserveAspectCrop - the video is scaled uniformly to fill, cropping if necessary
    \endlist

    The default fill mode is PreserveAspectFit.
*/

QDeclarativeVideoOutput::FillMode QDeclarativeVideoOutput::fillMode() const
{
    return m_fillMode;
}

void QDeclarativeVideoOutput::setFillMode(FillMode mode)
{
    if (mode == m_fillMode)
        return;

    m_fillMode = mode;
    m_geometryDirty = true;
    update();

    emit fillModeChanged(mode);
}

void QDeclarativeVideoOutput::_q_updateNativeSize()
{
    if (!m_backend)
        return;

    QSize size = m_backend->nativeSize();
    if (!qIsDefaultAspect(m_orientation)) {
        size.transpose();
    }

    if (m_nativeSize != size) {
        m_nativeSize = size;

        m_geometryDirty = true;

        setImplicitWidth(size.width());
        setImplicitHeight(size.height());

        emit sourceRectChanged();
    }
}

/* Based on fill mode and our size, figure out the source/dest rects */
void QDeclarativeVideoOutput::_q_updateGeometry()
{
    const QRectF rect(0, 0, width(), height());
    const QRectF absoluteRect(x(), y(), width(), height());

    if (!m_geometryDirty && m_lastRect == absoluteRect)
        return;

    QRectF oldContentRect(m_contentRect);

    m_geometryDirty = false;
    m_lastRect = absoluteRect;

    if (m_nativeSize.isEmpty()) {
        //this is necessary for item to receive the
        //first paint event and configure video surface.
        m_contentRect = rect;
    } else if (m_fillMode == Stretch) {
        m_contentRect = rect;
    } else if (m_fillMode == PreserveAspectFit || m_fillMode == PreserveAspectCrop) {
        QSizeF scaled = m_nativeSize;
        scaled.scale(rect.size(), m_fillMode == PreserveAspectFit ?
                         Qt::KeepAspectRatio : Qt::KeepAspectRatioByExpanding);

        m_contentRect = QRectF(QPointF(), scaled);
        m_contentRect.moveCenter(rect.center());
    }

    if (m_backend)
        m_backend->updateGeometry();

    if (m_contentRect != oldContentRect)
        emit contentRectChanged();
}

void QDeclarativeVideoOutput::_q_screenOrientationChanged(int orientation)
{
    // If the source is a camera, take into account its sensor position and orientation
    if (!m_cameraInfo.isNull()) {
        switch (m_cameraInfo.position()) {
        case QCamera::FrontFace:
            // Front facing cameras are flipped horizontally, compensate the mirror
            orientation += (360 - m_cameraInfo.orientation());
            break;
        case QCamera::BackFace:
        default:
            orientation += m_cameraInfo.orientation();
            break;
        }
    }

    setOrientation(orientation % 360);
}

/*!
    \qmlproperty int QtMultimedia::VideoOutput::orientation

    In some cases the source video stream requires a certain
    orientation to be correct.  This includes
    sources like a camera viewfinder, where the displayed
    viewfinder should match reality, no matter what rotation
    the rest of the user interface has.

    This property allows you to apply a rotation (in steps
    of 90 degrees) to compensate for any user interface
    rotation, with positive values in the anti-clockwise direction.

    The orientation change will also affect the mapping
    of coordinates from source to viewport.

    \sa autoOrientation
*/
int QDeclarativeVideoOutput::orientation() const
{
    return m_orientation;
}

void QDeclarativeVideoOutput::setOrientation(int orientation)
{
    // Make sure it's a multiple of 90.
    if (orientation % 90)
        return;

    // If there's no actual change, return
    if (m_orientation == orientation)
        return;

    // If the new orientation is the same effect
    // as the old one, don't update the video node stuff
    if ((m_orientation % 360) == (orientation % 360)) {
        m_orientation = orientation;
        emit orientationChanged();
        return;
    }

    m_geometryDirty = true;

    // Otherwise, a new orientation
    // See if we need to change aspect ratio orientation too
    bool oldAspect = qIsDefaultAspect(m_orientation);
    bool newAspect = qIsDefaultAspect(orientation);

    m_orientation = orientation;

    if (oldAspect != newAspect) {
        m_nativeSize.transpose();

        setImplicitWidth(m_nativeSize.width());
        setImplicitHeight(m_nativeSize.height());

        // Source rectangle does not change for orientation
    }

    update();
    emit orientationChanged();
}

/*!
    \qmlproperty bool QtMultimedia::VideoOutput::autoOrientation

    This property allows you to enable and disable auto orientation
    of the video stream, so that its orientation always matches
    the orientation of the screen. If \c autoOrientation is enabled,
    the \c orientation property is overwritten.

    By default \c autoOrientation is disabled.

    \sa orientation
    \since QtMultimedia 5.2
*/
bool QDeclarativeVideoOutput::autoOrientation() const
{
    return m_autoOrientation;
}

void QDeclarativeVideoOutput::setAutoOrientation(bool autoOrientation)
{
    if (autoOrientation == m_autoOrientation)
        return;

    m_autoOrientation = autoOrientation;
    if (m_autoOrientation) {
        m_screenOrientationHandler = new QVideoOutputOrientationHandler(this);
        connect(m_screenOrientationHandler, SIGNAL(orientationChanged(int)),
                this, SLOT(_q_screenOrientationChanged(int)));

        _q_screenOrientationChanged(m_screenOrientationHandler->currentOrientation());
    } else {
        disconnect(m_screenOrientationHandler, SIGNAL(orientationChanged(int)),
                   this, SLOT(_q_screenOrientationChanged(int)));
        m_screenOrientationHandler->deleteLater();
        m_screenOrientationHandler = 0;
    }

    emit autoOrientationChanged();
}

/*!
    \qmlproperty rectangle QtMultimedia::VideoOutput::contentRect

    This property holds the item coordinates of the area that
    would contain video to render.  With certain fill modes,
    this rectangle will be larger than the visible area of the
    \c VideoOutput.

    This property is useful when other coordinates are specified
    in terms of the source dimensions - this applied for relative
    (normalized) frame coordinates in the range of 0 to 1.0.

    \sa mapRectToItem(), mapPointToItem()

    Areas outside this will be transparent.
*/
QRectF QDeclarativeVideoOutput::contentRect() const
{
    return m_contentRect;
}

/*!
    \qmlproperty rectangle QtMultimedia::VideoOutput::sourceRect

    This property holds the area of the source video
    content that is considered for rendering.  The
    values are in source pixel coordinates, adjusted for
    the source's pixel aspect ratio.

    Note that typically the top left corner of this rectangle
    will be \c {0,0} while the width and height will be the
    width and height of the input content. Only when the video
    source has a viewport set, these values will differ.

    The orientation setting does not affect this rectangle.

    \sa QVideoSurfaceFormat::pixelAspectRatio()
    \sa QVideoSurfaceFormat::viewport()
*/
QRectF QDeclarativeVideoOutput::sourceRect() const
{
    // We might have to transpose back
    QSizeF size = m_nativeSize;
    if (!qIsDefaultAspect(m_orientation)) {
        size.transpose();
    }

    // No backend? Just assume no viewport.
    if (!m_nativeSize.isValid() || !m_backend) {
        return QRectF(QPointF(), size);
    }

    // Take the viewport into account for the top left position.
    // m_nativeSize is already adjusted to the viewport, as it originats
    // from QVideoSurfaceFormat::sizeHint(), which includes pixel aspect
    // ratio and viewport.
    const QRectF viewport = m_backend->adjustedViewport();
    Q_ASSERT(viewport.size() == size);
    return QRectF(viewport.topLeft(), size);
}

/*!
    \qmlmethod QPointF QtMultimedia::VideoOutput::mapNormalizedPointToItem (const QPointF &point) const

    Given normalized coordinates \a point (that is, each
    component in the range of 0 to 1.0), return the mapped point
    that it corresponds to (in item coordinates).
    This mapping is affected by the orientation.

    Depending on the fill mode, this point may lie outside the rendered
    rectangle.
 */
QPointF QDeclarativeVideoOutput::mapNormalizedPointToItem(const QPointF &point) const
{
    qreal dx = point.x();
    qreal dy = point.y();

    if (qIsDefaultAspect(m_orientation)) {
        dx *= m_contentRect.width();
        dy *= m_contentRect.height();
    } else {
        dx *= m_contentRect.height();
        dy *= m_contentRect.width();
    }

    switch (qNormalizedOrientation(m_orientation)) {
        case 0:
        default:
            return m_contentRect.topLeft() + QPointF(dx, dy);
        case 90:
            return m_contentRect.bottomLeft() + QPointF(dy, -dx);
        case 180:
            return m_contentRect.bottomRight() + QPointF(-dx, -dy);
        case 270:
            return m_contentRect.topRight() + QPointF(-dy, dx);
    }
}

/*!
    \qmlmethod QRectF QtMultimedia::VideoOutput::mapNormalizedRectToItem(const QRectF &rectangle) const

    Given a rectangle \a rectangle in normalized
    coordinates (that is, each component in the range of 0 to 1.0),
    return the mapped rectangle that it corresponds to (in item coordinates).
    This mapping is affected by the orientation.

    Depending on the fill mode, this rectangle may extend outside the rendered
    rectangle.
 */
QRectF QDeclarativeVideoOutput::mapNormalizedRectToItem(const QRectF &rectangle) const
{
    return QRectF(mapNormalizedPointToItem(rectangle.topLeft()),
                  mapNormalizedPointToItem(rectangle.bottomRight())).normalized();
}

/*!
    \qmlmethod QPointF QtMultimedia::VideoOutput::mapPointToSource(const QPointF &point) const

    Given a point \a point in item coordinates, return the
    corresponding point in source coordinates.  This mapping is
    affected by the orientation.

    If the supplied point lies outside the rendered area, the returned
    point will be outside the source rectangle.
 */
QPointF QDeclarativeVideoOutput::mapPointToSource(const QPointF &point) const
{
    QPointF norm = mapPointToSourceNormalized(point);

    if (qIsDefaultAspect(m_orientation))
        return QPointF(norm.x() * m_nativeSize.width(), norm.y() * m_nativeSize.height());
    else
        return QPointF(norm.x() * m_nativeSize.height(), norm.y() * m_nativeSize.width());
}

/*!
    \qmlmethod QRectF QtMultimedia::VideoOutput::mapRectToSource(const QRectF &rectangle) const

    Given a rectangle \a rectangle in item coordinates, return the
    corresponding rectangle in source coordinates.  This mapping is
    affected by the orientation.

    This mapping is affected by the orientation.

    If the supplied point lies outside the rendered area, the returned
    point will be outside the source rectangle.
 */
QRectF QDeclarativeVideoOutput::mapRectToSource(const QRectF &rectangle) const
{
    return QRectF(mapPointToSource(rectangle.topLeft()),
                  mapPointToSource(rectangle.bottomRight())).normalized();
}

/*!
    \qmlmethod QPointF QtMultimedia::VideoOutput::mapPointToSourceNormalized(const QPointF &point) const

    Given a point \a point in item coordinates, return the
    corresponding point in normalized source coordinates.  This mapping is
    affected by the orientation.

    If the supplied point lies outside the rendered area, the returned
    point will be outside the source rectangle.  No clamping is performed.
 */
QPointF QDeclarativeVideoOutput::mapPointToSourceNormalized(const QPointF &point) const
{
    if (m_contentRect.isEmpty())
        return QPointF();

    // Normalize the item source point
    qreal nx = (point.x() - m_contentRect.left()) / m_contentRect.width();
    qreal ny = (point.y() - m_contentRect.top()) / m_contentRect.height();

    const qreal one(1.0f);

    // For now, the origin of the source rectangle is 0,0
    switch (qNormalizedOrientation(m_orientation)) {
        case 0:
        default:
            return QPointF(nx, ny);
        case 90:
            return QPointF(one - ny, nx);
        case 180:
            return QPointF(one - nx, one - ny);
        case 270:
            return QPointF(ny, one - nx);
    }
}

/*!
    \qmlmethod QRectF QtMultimedia::VideoOutput::mapRectToSourceNormalized(const QRectF &rectangle) const

    Given a rectangle \a rectangle in item coordinates, return the
    corresponding rectangle in normalized source coordinates.  This mapping is
    affected by the orientation.

    This mapping is affected by the orientation.

    If the supplied point lies outside the rendered area, the returned
    point will be outside the source rectangle.  No clamping is performed.
 */
QRectF QDeclarativeVideoOutput::mapRectToSourceNormalized(const QRectF &rectangle) const
{
    return QRectF(mapPointToSourceNormalized(rectangle.topLeft()),
                  mapPointToSourceNormalized(rectangle.bottomRight())).normalized();
}

QDeclarativeVideoOutput::SourceType QDeclarativeVideoOutput::sourceType() const
{
    return m_sourceType;
}

/*!
    \qmlmethod QPointF QtMultimedia::VideoOutput::mapPointToItem(const QPointF &point) const

    Given a point \a point in source coordinates, return the
    corresponding point in item coordinates.  This mapping is
    affected by the orientation.

    Depending on the fill mode, this point may lie outside the rendered
    rectangle.
 */
QPointF QDeclarativeVideoOutput::mapPointToItem(const QPointF &point) const
{
    if (m_nativeSize.isEmpty())
        return QPointF();

    // Just normalize and use that function
    // m_nativeSize is transposed in some orientations
    if (qIsDefaultAspect(m_orientation))
        return mapNormalizedPointToItem(QPointF(point.x() / m_nativeSize.width(), point.y() / m_nativeSize.height()));
    else
        return mapNormalizedPointToItem(QPointF(point.x() / m_nativeSize.height(), point.y() / m_nativeSize.width()));
}

/*!
    \qmlmethod QRectF QtMultimedia::VideoOutput::mapRectToItem(const QRectF &rectangle) const

    Given a rectangle \a rectangle in source coordinates, return the
    corresponding rectangle in item coordinates.  This mapping is
    affected by the orientation.

    Depending on the fill mode, this rectangle may extend outside the rendered
    rectangle.

 */
QRectF QDeclarativeVideoOutput::mapRectToItem(const QRectF &rectangle) const
{
    return QRectF(mapPointToItem(rectangle.topLeft()),
                  mapPointToItem(rectangle.bottomRight())).normalized();
}

QSGNode *QDeclarativeVideoOutput::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    _q_updateGeometry();

    if (!m_backend)
        return 0;

    return m_backend->updatePaintNode(oldNode, data);
}

void QDeclarativeVideoOutput::itemChange(QQuickItem::ItemChange change,
                                         const QQuickItem::ItemChangeData &changeData)
{
    if (m_backend)
        m_backend->itemChange(change, changeData);
}

void QDeclarativeVideoOutput::releaseResources()
{
    if (m_backend)
        m_backend->releaseResources();
}

void QDeclarativeVideoOutput::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_UNUSED(newGeometry);
    Q_UNUSED(oldGeometry);

    QQuickItem::geometryChanged(newGeometry, oldGeometry);

    // Explicitly listen to geometry changes here. This is needed since changing the position does
    // not trigger a call to updatePaintNode().
    // We need to react to position changes though, as the window backened's display rect gets
    // changed in that situation.
    _q_updateGeometry();
}

/*!
    \qmlproperty list<object> QtMultimedia::VideoOutput::filters

    This property holds the list of video filters that are run on the video
    frames. The order of the filters in the list matches the order in which
    they will be invoked on the video frames. The objects in the list must be
    instances of a subclass of QAbstractVideoFilter.

    \sa QAbstractVideoFilter
*/

QQmlListProperty<QAbstractVideoFilter> QDeclarativeVideoOutput::filters()
{
    return QQmlListProperty<QAbstractVideoFilter>(this, 0, filter_append, filter_count, filter_at, filter_clear);
}

void QDeclarativeVideoOutput::filter_append(QQmlListProperty<QAbstractVideoFilter> *property, QAbstractVideoFilter *value)
{
    QDeclarativeVideoOutput *self = static_cast<QDeclarativeVideoOutput *>(property->object);
    self->m_filters.append(value);
    if (self->m_backend)
        self->m_backend->appendFilter(value);
}

int QDeclarativeVideoOutput::filter_count(QQmlListProperty<QAbstractVideoFilter> *property)
{
    QDeclarativeVideoOutput *self = static_cast<QDeclarativeVideoOutput *>(property->object);
    return self->m_filters.count();
}

QAbstractVideoFilter *QDeclarativeVideoOutput::filter_at(QQmlListProperty<QAbstractVideoFilter> *property, int index)
{
    QDeclarativeVideoOutput *self = static_cast<QDeclarativeVideoOutput *>(property->object);
    return self->m_filters.at(index);
}

void QDeclarativeVideoOutput::filter_clear(QQmlListProperty<QAbstractVideoFilter> *property)
{
    QDeclarativeVideoOutput *self = static_cast<QDeclarativeVideoOutput *>(property->object);
    self->m_filters.clear();
    if (self->m_backend)
        self->m_backend->clearFilters();
}

void QDeclarativeVideoOutput::_q_invalidateSceneGraph()
{
    if (m_backend)
        m_backend->invalidateSceneGraph();
}

QT_END_NAMESPACE
