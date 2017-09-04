/****************************************************************************
**
** Copyright (C) 2016 Canonical Ltd.
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

#include "qgstreamermirtexturerenderer_p.h"

#include <qgstreamerplayersession.h>
#include <private/qvideosurfacegstsink_p.h>
#include <private/qgstutils_p.h>
#include <qabstractvideosurface.h>

#include <QAbstractVideoBuffer>
#include <QGuiApplication>
#include <QDebug>
#include <QtQuick/QQuickWindow>
#include <QOpenGLContext>
#include <QGLContext>
#include <QGuiApplication>
#include <qgl.h>

#include <gst/gst.h>

static QGstreamerMirTextureRenderer *rendererInstance = NULL;

class QGstreamerMirTextureBuffer : public QAbstractVideoBuffer
{
public:
    QGstreamerMirTextureBuffer(GLuint textureId) :
        QAbstractVideoBuffer(QAbstractVideoBuffer::GLTextureHandle),
        m_textureId(textureId)
    {
    }

    MapMode mapMode() const { return NotMapped; }

    uchar *map(MapMode mode, int *numBytes, int *bytesPerLine)
    {
        qDebug() << Q_FUNC_INFO;
        Q_UNUSED(mode);
        Q_UNUSED(numBytes);
        Q_UNUSED(bytesPerLine);

        return NULL;
    }

    void unmap() { qDebug() << Q_FUNC_INFO; }

    QVariant handle() const { return QVariant::fromValue<unsigned int>(m_textureId); }

    GLuint textureId() { return m_textureId; }

private:
    GLuint m_textureId;
};

QGstreamerMirTextureRenderer::QGstreamerMirTextureRenderer(QObject *parent
        , const QGstreamerPlayerSession *playerSession)
    : QVideoRendererControl(0), m_videoSink(0), m_surface(0),
      m_glSurface(0),
      m_context(0),
      m_glContext(0),
      m_textureId(0),
      m_offscreenSurface(0),
      m_textureBuffer(0)
{
    Q_UNUSED(parent);
    setPlayerSession(playerSession);
}

QGstreamerMirTextureRenderer::~QGstreamerMirTextureRenderer()
{
    if (m_videoSink)
        gst_object_unref(GST_OBJECT(m_videoSink));

    delete m_glContext;
    delete m_offscreenSurface;
}

GstElement *QGstreamerMirTextureRenderer::videoSink()
{
    qDebug() << Q_FUNC_INFO;

    // FIXME: Ugly hack until I figure out why passing this segfaults in the g_signal handler
    rendererInstance = const_cast<QGstreamerMirTextureRenderer*>(this);

    if (!m_videoSink && m_surface) {
        qDebug() << Q_FUNC_INFO << ": using mirsink, (this: " << this << ")";

        m_videoSink = gst_element_factory_make("mirsink", "video-output");

        connect(QGuiApplication::instance(), SIGNAL(focusWindowChanged(QWindow*)),
                this, SLOT(handleFocusWindowChanged(QWindow*)), Qt::QueuedConnection);

        g_signal_connect(G_OBJECT(m_videoSink), "frame-ready", G_CALLBACK(handleFrameReady),
                (gpointer)this);
    }

    if (m_videoSink) {
        gst_object_ref_sink(GST_OBJECT(m_videoSink));

        GstPad *pad = gst_element_get_static_pad(m_videoSink, "sink");
        gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER,
                padBufferProbe, this, NULL);
    }

    return m_videoSink;
}

QWindow *QGstreamerMirTextureRenderer::createOffscreenWindow(const QSurfaceFormat &format)
{
    QWindow *w = new QWindow();
    w->setSurfaceType(QWindow::OpenGLSurface);
    w->setFormat(format);
    w->setGeometry(0, 0, 1, 1);
    w->setFlags(w->flags() | Qt::WindowTransparentForInput);
    w->create();

    return w;
}

void QGstreamerMirTextureRenderer::handleFrameReady(gpointer userData)
{
    QGstreamerMirTextureRenderer *renderer = reinterpret_cast<QGstreamerMirTextureRenderer*>(userData);
#if 1
    QMutexLocker locker(&rendererInstance->m_mutex);
    QMetaObject::invokeMethod(rendererInstance, "renderFrame", Qt::QueuedConnection);
#else
    // FIXME!
    //QMutexLocker locker(&renderer->m_mutex);
    QMetaObject::invokeMethod(renderer, "renderFrame", Qt::QueuedConnection);
#endif
}

void QGstreamerMirTextureRenderer::renderFrame()
{
    //qDebug() << Q_FUNC_INFO;

    if (m_context)
        m_context->makeCurrent();

    GstState pendingState = GST_STATE_NULL;
    GstState newState = GST_STATE_NULL;
    // Don't block and return immediately:
    GstStateChangeReturn ret = gst_element_get_state(m_videoSink, &newState,
                                                     &pendingState, 0);
    if (ret == GST_STATE_CHANGE_FAILURE || newState == GST_STATE_NULL||
            pendingState == GST_STATE_NULL) {
        qWarning() << "Invalid state change for renderer, aborting";
        stopRenderer();
        return;
    }

    if (!m_surface->isActive()) {
        qDebug() << "m_surface is not active";
        GstPad *pad = gst_element_get_static_pad(m_videoSink, "sink");
        GstCaps *caps = gst_pad_get_current_caps(pad);

        if (caps) {
            // Get the native video size from the video sink
            QSize newNativeSize = QGstUtils::capsCorrectedResolution(caps);
            if (m_nativeSize != newNativeSize) {
                m_nativeSize = newNativeSize;
                emit nativeSizeChanged();
            }
            gst_caps_unref(caps);
        }

        // Start the surface
        QVideoSurfaceFormat format(m_nativeSize, QVideoFrame::Format_RGB32, QAbstractVideoBuffer::GLTextureHandle);
        qDebug() << "m_nativeSize: " << m_nativeSize;
        qDebug() << "format: " << format;
        if (!m_surface->start(format)) {
            qWarning() << Q_FUNC_INFO << ": failed to start the video surface " << format;
            return;
        }
    }

    QGstreamerMirTextureBuffer *buffer = new QGstreamerMirTextureBuffer(m_textureId);
    //qDebug() << "frameSize: " << m_surface->surfaceFormat().frameSize();
    QVideoFrame frame(buffer, m_surface->surfaceFormat().frameSize(),
                      m_surface->surfaceFormat().pixelFormat());

    frame.setMetaData("TextureId", m_textureId);

    // Display the video frame on the surface:
    m_surface->present(frame);
}

GstPadProbeReturn QGstreamerMirTextureRenderer::padBufferProbe(GstPad *pad, GstPadProbeInfo *info, gpointer userData)
{
    Q_UNUSED(pad);
    Q_UNUSED(info);

    QGstreamerMirTextureRenderer *control = reinterpret_cast<QGstreamerMirTextureRenderer*>(userData);
    QMetaObject::invokeMethod(control, "updateNativeVideoSize", Qt::QueuedConnection);

    return GST_PAD_PROBE_REMOVE;
}

void QGstreamerMirTextureRenderer::stopRenderer()
{
    if (m_surface)
        m_surface->stop();
}

QAbstractVideoSurface *QGstreamerMirTextureRenderer::surface() const
{
    return m_surface;
}

void QGstreamerMirTextureRenderer::setSurface(QAbstractVideoSurface *surface)
{
    qDebug() << Q_FUNC_INFO;

    if (m_surface != surface) {
        qDebug() << "Saving current QGLContext";
        m_context = const_cast<QGLContext*>(QGLContext::currentContext());

        if (m_videoSink)
            gst_object_unref(GST_OBJECT(m_videoSink));

        m_videoSink = 0;

        if (m_surface) {
            disconnect(m_surface.data(), SIGNAL(supportedFormatsChanged()),
                       this, SLOT(handleFormatChange()));
        }

        bool wasReady = isReady();

        m_surface = surface;

        if (m_surface) {
            connect(m_surface.data(), SIGNAL(supportedFormatsChanged()),
                    this, SLOT(handleFormatChange()));
        }

        if (wasReady != isReady())
            emit readyChanged(isReady());

        emit sinkChanged();
    }
}

void QGstreamerMirTextureRenderer::setPlayerSession(const QGstreamerPlayerSession *playerSession)
{
    m_playerSession = const_cast<QGstreamerPlayerSession*>(playerSession);
}

void QGstreamerMirTextureRenderer::handleFormatChange()
{
    qDebug() << "Supported formats list has changed, reload video output";

    if (m_videoSink)
        gst_object_unref(GST_OBJECT(m_videoSink));

    m_videoSink = 0;
    emit sinkChanged();
}

void QGstreamerMirTextureRenderer::updateNativeVideoSize()
{
    //qDebug() << Q_FUNC_INFO;
    const QSize oldSize = m_nativeSize;

    if (m_videoSink) {
        // Find video native size to update video widget size hint
        GstPad *pad = gst_element_get_static_pad(m_videoSink,"sink");
        GstCaps *caps = gst_pad_get_current_caps(pad);

        if (caps) {
            m_nativeSize = QGstUtils::capsCorrectedResolution(caps);
            gst_caps_unref(caps);
        }
    } else {
        m_nativeSize = QSize();
    }
    qDebug() << Q_FUNC_INFO << oldSize << m_nativeSize << m_videoSink;

    if (m_nativeSize != oldSize)
        emit nativeSizeChanged();
}

void QGstreamerMirTextureRenderer::handleFocusWindowChanged(QWindow *window)
{
    qDebug() << Q_FUNC_INFO;

    QOpenGLContext *currContext = QOpenGLContext::currentContext();

    QQuickWindow *w = dynamic_cast<QQuickWindow*>(window);
    // If we don't have a GL context in the current thread, create one and share it
    // with the render thread GL context
    if (!currContext && !m_glContext) {
        // This emulates the new QOffscreenWindow class with Qt5.1
        m_offscreenSurface = createOffscreenWindow(w->openglContext()->surface()->format());
        m_offscreenSurface->setParent(window);

        QOpenGLContext *shareContext = 0;
        if (m_surface)
            shareContext = qobject_cast<QOpenGLContext*>(m_surface->property("GLContext").value<QObject*>());
        m_glContext = new QOpenGLContext;
        m_glContext->setFormat(m_offscreenSurface->requestedFormat());

        if (shareContext)
            m_glContext->setShareContext(shareContext);

        if (!m_glContext->create())
        {
            qWarning() << "Failed to create new shared context.";
            return;
        }
    }

    if (m_glContext)
        m_glContext->makeCurrent(m_offscreenSurface);

    if (m_textureId == 0) {
        glGenTextures(1, &m_textureId);
        qDebug() << "texture_id (handleFocusWindowChanged): " << m_textureId << endl;
        g_object_set(G_OBJECT(m_videoSink), "texture-id", m_textureId, (char*)NULL);
    }
}
