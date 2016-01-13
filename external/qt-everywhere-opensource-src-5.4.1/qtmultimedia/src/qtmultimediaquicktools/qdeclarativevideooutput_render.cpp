/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Copyright (C) 2012 Research In Motion
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
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

#include "qdeclarativevideooutput_render_p.h"
#include "qdeclarativevideooutput_p.h"
#include <QtMultimedia/qvideorenderercontrol.h>
#include <QtMultimedia/qmediaservice.h>
#include <QtCore/qloggingcategory.h>
#include <private/qmediapluginloader_p.h>
#include <private/qsgvideonode_p.h>

#include <QtGui/QOpenGLContext>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(qLcVideo)

Q_GLOBAL_STATIC_WITH_ARGS(QMediaPluginLoader, videoNodeFactoryLoader,
        (QSGVideoNodeFactoryInterface_iid, QLatin1String("video/videonode"), Qt::CaseInsensitive))

QDeclarativeVideoRendererBackend::QDeclarativeVideoRendererBackend(QDeclarativeVideoOutput *parent)
    : QDeclarativeVideoBackend(parent),
      m_glContext(0),
      m_frameChanged(false)
{
    m_surface = new QSGVideoItemSurface(this);
    QObject::connect(m_surface, SIGNAL(surfaceFormatChanged(QVideoSurfaceFormat)),
                     q, SLOT(_q_updateNativeSize()), Qt::QueuedConnection);

    // Prioritize the plugin requested by the environment
    QString requestedVideoNode = QString::fromLatin1(qgetenv("QT_VIDEONODE"));

    foreach (const QString &key, videoNodeFactoryLoader()->keys()) {
        QObject *instance = videoNodeFactoryLoader()->instance(key);
        QSGVideoNodeFactoryInterface* plugin = qobject_cast<QSGVideoNodeFactoryInterface*>(instance);
        if (plugin) {
            if (key == requestedVideoNode)
                m_videoNodeFactories.prepend(plugin);
            else
                m_videoNodeFactories.append(plugin);
            qCDebug(qLcVideo) << "found videonode plugin" << key << plugin;
        }
    }

    // Append existing node factories as fallback if we have no plugins
    m_videoNodeFactories.append(&m_i420Factory);
    m_videoNodeFactories.append(&m_rgbFactory);
    m_videoNodeFactories.append(&m_textureFactory);
}

QDeclarativeVideoRendererBackend::~QDeclarativeVideoRendererBackend()
{
    releaseSource();
    releaseControl();
    delete m_surface;
}

bool QDeclarativeVideoRendererBackend::init(QMediaService *service)
{
    // When there is no service, the source is an object with a "videoSurface" property, which
    // doesn't require a QVideoRendererControl and therefore always works
    if (!service)
        return true;

    if (QMediaControl *control = service->requestControl(QVideoRendererControl_iid)) {
        if ((m_rendererControl = qobject_cast<QVideoRendererControl *>(control))) {
            m_rendererControl->setSurface(m_surface);
            m_service = service;
            return true;
        }
    }
    return false;
}

void QDeclarativeVideoRendererBackend::itemChange(QQuickItem::ItemChange change,
                                      const QQuickItem::ItemChangeData &changeData)
{
    Q_UNUSED(change);
    Q_UNUSED(changeData);
}

void QDeclarativeVideoRendererBackend::releaseSource()
{
    if (q->source() && q->sourceType() == QDeclarativeVideoOutput::VideoSurfaceSource) {
        if (q->source()->property("videoSurface").value<QAbstractVideoSurface*>() == m_surface)
            q->source()->setProperty("videoSurface", QVariant::fromValue<QAbstractVideoSurface*>(0));
    }

    m_surface->stop();
}

void QDeclarativeVideoRendererBackend::releaseControl()
{
    if (m_rendererControl) {
        m_rendererControl->setSurface(0);
        if (m_service)
            m_service->releaseControl(m_rendererControl);
        m_rendererControl = 0;
    }
}

QSize QDeclarativeVideoRendererBackend::nativeSize() const
{
    return m_surface->surfaceFormat().sizeHint();
}

void QDeclarativeVideoRendererBackend::updateGeometry()
{
    const QRectF viewport = videoSurface()->surfaceFormat().viewport();
    const QSizeF frameSize = videoSurface()->surfaceFormat().frameSize();
    const QRectF normalizedViewport(viewport.x() / frameSize.width(),
                                    viewport.y() / frameSize.height(),
                                    viewport.width() / frameSize.width(),
                                    viewport.height() / frameSize.height());
    const QRectF rect(0, 0, q->width(), q->height());
    if (nativeSize().isEmpty()) {
        m_renderedRect = rect;
        m_sourceTextureRect = normalizedViewport;
    } else if (q->fillMode() == QDeclarativeVideoOutput::Stretch) {
        m_renderedRect = rect;
        m_sourceTextureRect = normalizedViewport;
    } else if (q->fillMode() == QDeclarativeVideoOutput::PreserveAspectFit) {
        m_sourceTextureRect = normalizedViewport;
        m_renderedRect = q->contentRect();
    } else if (q->fillMode() == QDeclarativeVideoOutput::PreserveAspectCrop) {
        m_renderedRect = rect;
        const qreal contentHeight = q->contentRect().height();
        const qreal contentWidth = q->contentRect().width();

        // Calculate the size of the source rectangle without taking the viewport into account
        const qreal relativeOffsetLeft = -q->contentRect().left() / contentWidth;
        const qreal relativeOffsetTop = -q->contentRect().top() / contentHeight;
        const qreal relativeWidth = rect.width() / contentWidth;
        const qreal relativeHeight = rect.height() / contentHeight;

        // Now take the viewport size into account
        const qreal totalOffsetLeft = normalizedViewport.x() + relativeOffsetLeft * normalizedViewport.width();
        const qreal totalOffsetTop = normalizedViewport.y() + relativeOffsetTop * normalizedViewport.height();
        const qreal totalWidth = normalizedViewport.width() * relativeWidth;
        const qreal totalHeight = normalizedViewport.height() * relativeHeight;

        if (qIsDefaultAspect(q->orientation())) {
            m_sourceTextureRect = QRectF(totalOffsetLeft, totalOffsetTop,
                                         totalWidth, totalHeight);
        } else {
            m_sourceTextureRect = QRectF(totalOffsetTop, totalOffsetLeft,
                                         totalHeight, totalWidth);
        }
    }

    if (videoSurface()->surfaceFormat().scanLineDirection() == QVideoSurfaceFormat::BottomToTop) {
        qreal top = m_sourceTextureRect.top();
        m_sourceTextureRect.setTop(m_sourceTextureRect.bottom());
        m_sourceTextureRect.setBottom(top);
    }
}

QSGNode *QDeclarativeVideoRendererBackend::updatePaintNode(QSGNode *oldNode,
                                                           QQuickItem::UpdatePaintNodeData *data)
{
    Q_UNUSED(data);
    QSGVideoNode *videoNode = static_cast<QSGVideoNode *>(oldNode);

    QMutexLocker lock(&m_frameMutex);

    if (!m_glContext) {
        m_glContext = QOpenGLContext::currentContext();
        m_surface->scheduleOpenGLContextUpdate();

        // Internal mechanism to call back the surface renderer from the QtQuick render thread
        QObject *obj = m_surface->property("_q_GLThreadCallback").value<QObject*>();
        if (obj) {
            QEvent ev(QEvent::User);
            obj->event(&ev);
        }
    }
#if defined (Q_OS_QNX) // On QNX we need to be called back again for creating the egl images
    else {
        // Internal mechanism to call back the surface renderer from the QtQuick render thread
        QObject *obj = m_surface->property("_q_GLThreadCallback").value<QObject*>();
        if (obj) {
            QEvent ev(static_cast<QEvent::Type>(QEvent::User + 1));
            obj->event(&ev);
        }
    }
#endif

    if (m_frameChanged) {
        if (videoNode && videoNode->pixelFormat() != m_frame.pixelFormat()) {
            qCDebug(qLcVideo) << "updatePaintNode: deleting old video node because frame format changed";
            delete videoNode;
            videoNode = 0;
        }

        if (!m_frame.isValid()) {
            qCDebug(qLcVideo) << "updatePaintNode: no frames yet";
            m_frameChanged = false;
            return 0;
        }

        if (!videoNode) {
            foreach (QSGVideoNodeFactoryInterface* factory, m_videoNodeFactories) {
                videoNode = factory->createNode(m_surface->surfaceFormat());
                if (videoNode) {
                    qCDebug(qLcVideo) << "updatePaintNode: Video node created. Handle type:" << m_frame.handleType()
                                     << " Supported formats for the handle by this node:"
                                     << factory->supportedPixelFormats(m_frame.handleType());
                    break;
                }
            }
        }
    }

    if (!videoNode) {
        m_frameChanged = false;
        m_frame = QVideoFrame();
        return 0;
    }

    // Negative rotations need lots of %360
    videoNode->setTexturedRectGeometry(m_renderedRect, m_sourceTextureRect,
                                       qNormalizedOrientation(q->orientation()));
    if (m_frameChanged) {
        videoNode->setCurrentFrame(m_frame);
        //don't keep the frame for more than really necessary
        m_frameChanged = false;
        m_frame = QVideoFrame();
    }
    return videoNode;
}

QAbstractVideoSurface *QDeclarativeVideoRendererBackend::videoSurface() const
{
    return m_surface;
}

QRectF QDeclarativeVideoRendererBackend::adjustedViewport() const
{
    const QRectF viewport = m_surface->surfaceFormat().viewport();
    const QSize pixelAspectRatio = m_surface->surfaceFormat().pixelAspectRatio();

    if (pixelAspectRatio.height() != 0) {
        const qreal ratio = pixelAspectRatio.width() / pixelAspectRatio.height();
        QRectF result = viewport;
        result.setX(result.x() * ratio);
        result.setWidth(result.width() * ratio);
        return result;
    }

    return viewport;
}

QOpenGLContext *QDeclarativeVideoRendererBackend::glContext() const
{
    return m_glContext;
}

void QDeclarativeVideoRendererBackend::present(const QVideoFrame &frame)
{
    m_frameMutex.lock();
    m_frame = frame;
    m_frameChanged = true;
    m_frameMutex.unlock();

    q->update();
}

void QDeclarativeVideoRendererBackend::stop()
{
    present(QVideoFrame());
}

QSGVideoItemSurface::QSGVideoItemSurface(QDeclarativeVideoRendererBackend *backend, QObject *parent)
    : QAbstractVideoSurface(parent),
      m_backend(backend)
{
}

QSGVideoItemSurface::~QSGVideoItemSurface()
{
}

QList<QVideoFrame::PixelFormat> QSGVideoItemSurface::supportedPixelFormats(
        QAbstractVideoBuffer::HandleType handleType) const
{
    QList<QVideoFrame::PixelFormat> formats;

    foreach (QSGVideoNodeFactoryInterface* factory, m_backend->m_videoNodeFactories)
        formats.append(factory->supportedPixelFormats(handleType));

    return formats;
}

bool QSGVideoItemSurface::start(const QVideoSurfaceFormat &format)
{
    qCDebug(qLcVideo) << "Video surface format:" << format << "all supported formats:" << supportedPixelFormats(format.handleType());

    if (!supportedPixelFormats(format.handleType()).contains(format.pixelFormat()))
        return false;

    return QAbstractVideoSurface::start(format);
}

void QSGVideoItemSurface::stop()
{
    m_backend->stop();
    QAbstractVideoSurface::stop();
}

bool QSGVideoItemSurface::present(const QVideoFrame &frame)
{
    if (!frame.isValid()) {
        qWarning() << Q_FUNC_INFO << "I'm getting bad frames here...";
        return false;
    }
    m_backend->present(frame);
    return true;
}

void QSGVideoItemSurface::scheduleOpenGLContextUpdate()
{
    //This method is called from render thread
    QMetaObject::invokeMethod(this, "updateOpenGLContext");
}

void QSGVideoItemSurface::updateOpenGLContext()
{
    //Set a dynamic property to access the OpenGL context in Qt Quick render thread.
    this->setProperty("GLContext", QVariant::fromValue<QObject*>(m_backend->glContext()));
}

QT_END_NAMESPACE
