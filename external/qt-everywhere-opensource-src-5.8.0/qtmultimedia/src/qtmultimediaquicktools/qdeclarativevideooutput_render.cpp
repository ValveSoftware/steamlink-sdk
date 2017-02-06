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

#include "qdeclarativevideooutput_render_p.h"
#include "qdeclarativevideooutput_p.h"
#include <QtMultimedia/qabstractvideofilter.h>
#include <QtMultimedia/qvideorenderercontrol.h>
#include <QtMultimedia/qmediaservice.h>
#include <QtCore/qloggingcategory.h>
#include <private/qmediapluginloader_p.h>
#include <private/qsgvideonode_p.h>

#include <QtGui/QOpenGLContext>
#include <QtQuick/QQuickWindow>
#include <QtCore/QRunnable>

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

    const auto keys = videoNodeFactoryLoader()->keys();
    for (const QString &key : keys) {
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

void QDeclarativeVideoRendererBackend::appendFilter(QAbstractVideoFilter *filter)
{
    QMutexLocker lock(&m_frameMutex);
    m_filters.append(Filter(filter));
}

void QDeclarativeVideoRendererBackend::clearFilters()
{
    QMutexLocker lock(&m_frameMutex);
    scheduleDeleteFilterResources();
    m_filters.clear();
}

class FilterRunnableDeleter : public QRunnable
{
public:
    FilterRunnableDeleter(const QList<QVideoFilterRunnable *> &runnables) : m_runnables(runnables) { }
    void run() Q_DECL_OVERRIDE {
        for (QVideoFilterRunnable *runnable : qAsConst(m_runnables))
            delete runnable;
    }
private:
    QList<QVideoFilterRunnable *> m_runnables;
};

void QDeclarativeVideoRendererBackend::scheduleDeleteFilterResources()
{
    if (!q->window())
        return;

    QList<QVideoFilterRunnable *> runnables;
    for (int i = 0; i < m_filters.count(); ++i) {
        if (m_filters[i].runnable) {
            runnables.append(m_filters[i].runnable);
            m_filters[i].runnable = 0;
        }
    }

    if (!runnables.isEmpty()) {
        // Request the scenegraph to run our cleanup job on the render thread.
        // The execution of our QRunnable may happen after the QML tree including the QAbstractVideoFilter instance is
        // destroyed on the main thread so no references to it must be used during cleanup.
        q->window()->scheduleRenderJob(new FilterRunnableDeleter(runnables), QQuickWindow::BeforeSynchronizingStage);
    }
}

void QDeclarativeVideoRendererBackend::releaseResources()
{
    // Called on the gui thread when the window is closed or changed.
    QMutexLocker lock(&m_frameMutex);
    scheduleDeleteFilterResources();
}

void QDeclarativeVideoRendererBackend::invalidateSceneGraph()
{
    // Called on the render thread, e.g. when the context is lost.
    QMutexLocker lock(&m_frameMutex);
    for (int i = 0; i < m_filters.count(); ++i) {
        if (m_filters[i].runnable) {
            delete m_filters[i].runnable;
            m_filters[i].runnable = 0;
        }
    }
}

void QDeclarativeVideoRendererBackend::itemChange(QQuickItem::ItemChange change,
                                      const QQuickItem::ItemChangeData &changeData)
{
    if (change == QQuickItem::ItemSceneChange) {
        if (changeData.window)
            QObject::connect(changeData.window, SIGNAL(sceneGraphInvalidated()),
                             q, SLOT(_q_invalidateSceneGraph()), Qt::DirectConnection);
    }
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

    if (videoSurface()->surfaceFormat().property("mirrored").toBool()) {
        qreal left = m_sourceTextureRect.left();
        m_sourceTextureRect.setLeft(m_sourceTextureRect.right());
        m_sourceTextureRect.setRight(left);
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

    bool isFrameModified = false;
    if (m_frameChanged) {
        // Run the VideoFilter if there is one. This must be done before potentially changing the videonode below.
        if (m_frame.isValid() && !m_filters.isEmpty()) {
            const QVideoSurfaceFormat surfaceFormat = videoSurface()->surfaceFormat();
            for (int i = 0; i < m_filters.count(); ++i) {
                QAbstractVideoFilter *filter = m_filters[i].filter;
                QVideoFilterRunnable *&runnable = m_filters[i].runnable;
                if (filter && filter->isActive()) {
                    // Create the filter runnable if not yet done. Ownership is taken and is tied to this thread, on which rendering happens.
                    if (!runnable)
                        runnable = filter->createFilterRunnable();
                    if (!runnable)
                        continue;

                    QVideoFilterRunnable::RunFlags flags = 0;
                    if (i == m_filters.count() - 1)
                        flags |= QVideoFilterRunnable::LastInChain;

                    QVideoFrame newFrame = runnable->run(&m_frame, surfaceFormat, flags);

                    if (newFrame.isValid() && newFrame != m_frame) {
                        isFrameModified = true;
                        m_frame = newFrame;
                    }
                }
            }
        }

        if (videoNode && (videoNode->pixelFormat() != m_frame.pixelFormat() || videoNode->handleType() != m_frame.handleType())) {
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
            for (QSGVideoNodeFactoryInterface* factory : qAsConst(m_videoNodeFactories)) {
                // Get a node that supports our frame. The surface is irrelevant, our
                // QSGVideoItemSurface supports (logically) anything.
                videoNode = factory->createNode(QVideoSurfaceFormat(m_frame.size(), m_frame.pixelFormat(), m_frame.handleType()));
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
        QSGVideoNode::FrameFlags flags = 0;
        if (isFrameModified)
            flags |= QSGVideoNode::FrameFiltered;
        videoNode->setCurrentFrame(m_frame, flags);
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

    static bool noGLTextures = false;
    static bool noGLTexturesChecked = false;
    if (handleType == QAbstractVideoBuffer::GLTextureHandle) {
        if (!noGLTexturesChecked) {
            noGLTexturesChecked = true;
            noGLTextures = qEnvironmentVariableIsSet("QT_QUICK_NO_TEXTURE_VIDEOFRAMES");
        }
        if (noGLTextures)
            return formats;
    }

    for (QSGVideoNodeFactoryInterface* factory : qAsConst(m_backend->m_videoNodeFactories))
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
