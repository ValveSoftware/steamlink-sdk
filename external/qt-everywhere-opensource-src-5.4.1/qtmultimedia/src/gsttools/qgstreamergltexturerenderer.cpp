/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include <private/qvideosurfacegstsink_p.h>
#include <qabstractvideosurface.h>
#include <private/qgstutils_p.h>

#include <QtGui/qevent.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qx11info_x11.h>
#include <QtCore/qdebug.h>
#include <QtCore/qthread.h>

#include <QtOpenGL/qgl.h>

#include <gst/gst.h>
#include <gst/interfaces/xoverlay.h>
#include <gst/interfaces/propertyprobe.h>
#include <gst/interfaces/meegovideotexture.h>
#include <gst/interfaces/meegovideorenderswitch.h>


#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "qgstreamergltexturerenderer_p.h"

//#define GL_TEXTURE_SINK_DEBUG 1

//from extdefs.h
typedef void *EGLSyncKHR;
typedef khronos_utime_nanoseconds_t EGLTimeKHR;

#define GL_TEXTURE_EXTERNAL_OES                         0x8D65
#define EGL_SYNC_FENCE_KHR                              0x30F9

typedef EGLSyncKHR (EGLAPIENTRYP _PFNEGLCREATESYNCKHRPROC) (EGLDisplay dpy,
    EGLenum type, const EGLint * attrib_list);
typedef EGLBoolean (EGLAPIENTRYP _PFNEGLDESTROYSYNCKHRPROC) (EGLDisplay dpy,
    EGLSyncKHR sync);


const QAbstractVideoBuffer::HandleType EGLImageTextureHandle =
        QAbstractVideoBuffer::HandleType(QAbstractVideoBuffer::UserHandle+3434);

// EGLSync functions
_PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR;
_PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR;

class QGStreamerGLTextureBuffer : public QAbstractVideoBuffer
{
public:
    QGStreamerGLTextureBuffer(MeegoGstVideoTexture *textureSink, int frameNumber) :
        QAbstractVideoBuffer(EGLImageTextureHandle),
        m_textureSink(MEEGO_GST_VIDEO_TEXTURE(textureSink)),
        m_frameNumber(frameNumber)
    {
    }

    ~QGStreamerGLTextureBuffer()
    {
    }


    MapMode mapMode() const { return NotMapped; }
    uchar *map(MapMode mode, int *numBytes, int *bytesPerLine)
    {
        Q_UNUSED(mode);
        Q_UNUSED(numBytes);
        Q_UNUSED(bytesPerLine);

        //acquire_frame should really be called at buffer construction time
        //but it conflicts with id-less implementation of gst texture sink.
#if defined(GL_TEXTURE_SINK_DEBUG) && GL_TEXTURE_SINK_DEBUG > 1
        qDebug() << "acquire frame" << m_frameNumber;
#endif
        if (!meego_gst_video_texture_acquire_frame(m_textureSink,m_frameNumber))
            qWarning() << Q_FUNC_INFO << "acquire-frame failed" << m_frameNumber;


#if defined(GL_TEXTURE_SINK_DEBUG) && GL_TEXTURE_SINK_DEBUG > 1
        qDebug() << "map frame" << m_frameNumber;
#endif

        gboolean bind_status = meego_gst_video_texture_bind_frame(m_textureSink, GL_TEXTURE_EXTERNAL_OES, m_frameNumber);
        if (!bind_status)
            qWarning() << Q_FUNC_INFO << "bind-frame failed";

        return (uchar*)1;
    }

    void unmap()
    {
        gboolean bind_status = meego_gst_video_texture_bind_frame(m_textureSink, GL_TEXTURE_EXTERNAL_OES, -1);

#if defined(GL_TEXTURE_SINK_DEBUG) && GL_TEXTURE_SINK_DEBUG > 1
        qDebug() << "unmap frame" << m_frameNumber;
#endif

        if (!bind_status)
            qWarning() << Q_FUNC_INFO << "unbind-frame failed";

        //release_frame should really be called in destructor
        //but this conflicts with id-less implementation of gst texture sink.
#if defined(GL_TEXTURE_SINK_DEBUG) && GL_TEXTURE_SINK_DEBUG > 1
        qDebug() << "release frame" << m_frameNumber;
#endif
        EGLSyncKHR sync = eglCreateSyncKHR(eglGetDisplay((EGLNativeDisplayType)QX11Info::display()), EGL_SYNC_FENCE_KHR, NULL);
        meego_gst_video_texture_release_frame(m_textureSink, m_frameNumber, sync);
    }

    QVariant handle() const
    {
        return m_frameNumber;
    }

private:
    MeegoGstVideoTexture *m_textureSink;
    int m_frameNumber;
};


QGstreamerGLTextureRenderer::QGstreamerGLTextureRenderer(QObject *parent) :
    QVideoRendererControl(parent),
    m_videoSink(0),
    m_surface(0),
    m_context(0),
    m_winId(0),
    m_colorKey(49,0,49),
    m_overlayEnabled(false),
    m_bufferProbeId(-1)
{
    eglCreateSyncKHR =
            (_PFNEGLCREATESYNCKHRPROC)eglGetProcAddress("eglCreateSyncKHR");
    eglDestroySyncKHR =
            (_PFNEGLDESTROYSYNCKHRPROC)eglGetProcAddress("eglDestroySyncKHR");
}

QGstreamerGLTextureRenderer::~QGstreamerGLTextureRenderer()
{
    if (m_surface && m_surface->isActive())
        m_surface->stop();

    if (m_videoSink)
        gst_object_unref(GST_OBJECT(m_videoSink));
}

GstElement *QGstreamerGLTextureRenderer::videoSink()
{
    if (!m_videoSink && isReady()) {
        if (m_context && !m_surface->supportedPixelFormats(EGLImageTextureHandle).isEmpty()) {
#ifdef GL_TEXTURE_SINK_DEBUG
            qDebug() << Q_FUNC_INFO << ": using gltexture sink";
#endif
            if (m_context)
                m_context->makeCurrent();
            m_videoSink = gst_element_factory_make("gltexturesink", "egl-texture-sink");
            g_object_set(G_OBJECT(m_videoSink),
                         "x-display", QX11Info::display(),
                         "egl-display", eglGetDisplay((EGLNativeDisplayType)QX11Info::display()),
                         "egl-context", eglGetCurrentContext(),
                         "colorkey", m_colorKey.rgb(),
                         "autopaint-colorkey", false,
                         "use-framebuffer-memory", true,
                         "render-mode", m_overlayEnabled ? VIDEO_RENDERSWITCH_XOVERLAY_MODE
                                                         : VIDEO_RENDERSWITCH_TEXTURE_STREAMING_MODE,
                         (char*)NULL);

            g_signal_connect(G_OBJECT(m_videoSink), "frame-ready", G_CALLBACK(handleFrameReady), (gpointer)this);
        } else {
            qWarning() << Q_FUNC_INFO << ": Fallback to QVideoSurfaceGstSink since EGLImageTextureHandle is not supported";
            m_videoSink = reinterpret_cast<GstElement*>(QVideoSurfaceGstSink::createSink(m_surface));
        }

        if (m_videoSink) {
            gst_object_ref(GST_OBJECT(m_videoSink)); //Take ownership
            gst_object_sink(GST_OBJECT(m_videoSink));

            GstPad *pad = gst_element_get_static_pad(m_videoSink,"sink");
            m_bufferProbeId = gst_pad_add_buffer_probe(pad, G_CALLBACK(padBufferProbe), this);
            gst_object_unref(GST_OBJECT(pad));
        }
    }

    return m_videoSink;
}

QAbstractVideoSurface *QGstreamerGLTextureRenderer::surface() const
{
    return m_surface;
}

void QGstreamerGLTextureRenderer::setSurface(QAbstractVideoSurface *surface)
{
    if (m_surface != surface) {
#ifdef GL_TEXTURE_SINK_DEBUG
        qDebug() << Q_FUNC_INFO << surface;
#endif

        bool oldReady = isReady();

        m_context = const_cast<QGLContext*>(QGLContext::currentContext());

        if (m_videoSink)
            gst_object_unref(GST_OBJECT(m_videoSink));

        m_videoSink = 0;

        if (m_surface) {
            disconnect(m_surface, SIGNAL(supportedFormatsChanged()),
                       this, SLOT(handleFormatChange()));
        }

        m_surface = surface;

        if (oldReady != isReady())
            emit readyChanged(!oldReady);

        if (m_surface) {
            connect(m_surface, SIGNAL(supportedFormatsChanged()),
                    this, SLOT(handleFormatChange()));
        }

        emit sinkChanged();
    }
}

void QGstreamerGLTextureRenderer::handleFormatChange()
{
    if (m_videoSink)
        gst_object_unref(GST_OBJECT(m_videoSink));

    m_videoSink = 0;
    emit sinkChanged();
}

void QGstreamerGLTextureRenderer::handleFrameReady(GstElement *sink, gint frame, gpointer data)
{
    Q_UNUSED(sink);
    QGstreamerGLTextureRenderer* renderer = reinterpret_cast<QGstreamerGLTextureRenderer*>(data);

    QMutexLocker locker(&renderer->m_mutex);
    QMetaObject::invokeMethod(renderer, "renderGLFrame",
                              Qt::QueuedConnection,
                              Q_ARG(int, frame));

    //we have to wait to ensure the frame is not reused,
    //timeout is added to avoid deadlocks when the main thread is
    //waiting for rendering to complete, this is possible for example during state chages.
    //If frame is not rendered during 60ms (~1-2 frames interval) it's better to unblock and drop it if necessary
    renderer->m_renderCondition.wait(&renderer->m_mutex, 60);
}

void QGstreamerGLTextureRenderer::renderGLFrame(int frame)
{
#if defined(GL_TEXTURE_SINK_DEBUG) && GL_TEXTURE_SINK_DEBUG > 1
    qDebug() << Q_FUNC_INFO << "frame:" << frame << "surface active:" << m_surface->isActive();
#endif
    QMutexLocker locker(&m_mutex);

    if (!m_surface) {
        m_renderCondition.wakeAll();
        return;
    }

    MeegoGstVideoTexture *textureSink = MEEGO_GST_VIDEO_TEXTURE(m_videoSink);

    if (m_context)
        m_context->makeCurrent();

    //don't try to render the frame if state is changed to NULL or READY
    GstState pendingState = GST_STATE_NULL;
    GstState newState = GST_STATE_NULL;
    GstStateChangeReturn res = gst_element_get_state(m_videoSink,
                                                     &newState,
                                                     &pendingState,
                                                     0);//don't block and return immediately

    if (res == GST_STATE_CHANGE_FAILURE ||
            newState == GST_STATE_NULL ||
            pendingState == GST_STATE_NULL) {
        stopRenderer();
        m_renderCondition.wakeAll();
        return;
    }

    if (!m_surface->isActive()) {
        //find the native video size
        GstPad *pad = gst_element_get_static_pad(m_videoSink,"sink");
        GstCaps *caps = gst_pad_get_negotiated_caps(pad);

        if (caps) {
            QSize newNativeSize = QGstUtils::capsCorrectedResolution(caps);
            if (m_nativeSize != newNativeSize) {
                m_nativeSize = newNativeSize;
                emit nativeSizeChanged();
            }
            gst_caps_unref(caps);
        }

        //start the surface...
        QVideoSurfaceFormat format(m_nativeSize, QVideoFrame::Format_RGB32, EGLImageTextureHandle);
        if (!m_surface->start(format)) {
            qWarning() << Q_FUNC_INFO << "failed to start video surface" << format;
            m_renderCondition.wakeAll();
            return;
        }
    }

    QGStreamerGLTextureBuffer *buffer = new QGStreamerGLTextureBuffer(textureSink, frame);
    QVideoFrame videoFrame(buffer,
                           m_surface->surfaceFormat().frameSize(),
                           m_surface->surfaceFormat().pixelFormat());
    m_surface->present(videoFrame);
    m_renderCondition.wakeAll();
}

bool QGstreamerGLTextureRenderer::isReady() const
{
    if (!m_surface)
        return false;

    if (m_winId > 0)
        return true;

    //winId is required only for EGLImageTextureHandle compatible surfaces
    return m_surface->supportedPixelFormats(EGLImageTextureHandle).isEmpty();
}

bool QGstreamerGLTextureRenderer::processBusMessage(const QGstreamerMessage &message)
{
    GstMessage* gm = message.rawMessage();

#ifdef GL_TEXTURE_SINK_DEBUG
    qDebug() << Q_FUNC_INFO << GST_MESSAGE_TYPE_NAME(gm);
#endif

    if (GST_MESSAGE_TYPE(gm) == GST_MESSAGE_STATE_CHANGED &&
            GST_MESSAGE_SRC(gm) == GST_OBJECT_CAST(m_videoSink)) {
        GstState oldState;
        GstState newState;
        gst_message_parse_state_changed(gm, &oldState, &newState, 0);

#ifdef GL_TEXTURE_SINK_DEBUG
        qDebug() << Q_FUNC_INFO << "State changed:" << oldState << newState;
#endif

        if (newState == GST_STATE_READY || newState == GST_STATE_NULL) {
            stopRenderer();
        }

        if (oldState == GST_STATE_READY && newState == GST_STATE_PAUSED) {
            updateNativeVideoSize();
        }
    }

    return false;
}

bool QGstreamerGLTextureRenderer::processSyncMessage(const QGstreamerMessage &message)
{
    GstMessage* gm = message.rawMessage();

    if ((GST_MESSAGE_TYPE(gm) == GST_MESSAGE_ELEMENT) &&
            gst_structure_has_name(gm->structure, "prepare-xwindow-id") &&
            m_videoSink && GST_IS_X_OVERLAY(m_videoSink)) {
#ifdef GL_TEXTURE_SINK_DEBUG
    qDebug() << Q_FUNC_INFO;
#endif
        GstXOverlay *overlay = GST_X_OVERLAY(m_videoSink);

        gst_x_overlay_set_xwindow_id(overlay, m_winId);

        if (!m_displayRect.isEmpty()) {
            gst_x_overlay_set_render_rectangle(overlay,
                                               m_displayRect.x(),
                                               m_displayRect.y(),
                                               m_displayRect.width(),
                                               m_displayRect.height());
        }

        GstPad *pad = gst_element_get_static_pad(m_videoSink,"sink");
        m_bufferProbeId = gst_pad_add_buffer_probe(pad, G_CALLBACK(padBufferProbe), this);

        return true;
    }

    return false;
}

void QGstreamerGLTextureRenderer::stopRenderer()
{
#ifdef GL_TEXTURE_SINK_DEBUG
    qDebug() << Q_FUNC_INFO;
#endif

    if (m_surface && m_surface->isActive())
        m_surface->stop();

    if (!m_nativeSize.isEmpty()) {
        m_nativeSize = QSize();
        emit nativeSizeChanged();
    }
}

bool QGstreamerGLTextureRenderer::overlayEnabled() const
{
    return m_overlayEnabled;
}

void QGstreamerGLTextureRenderer::setOverlayEnabled(bool enabled)
{

    if (m_videoSink && (m_overlayEnabled != enabled)) {
        qDebug() << Q_FUNC_INFO << enabled;
        g_object_set(G_OBJECT(m_videoSink),
                     "render-mode",
                     enabled ? VIDEO_RENDERSWITCH_XOVERLAY_MODE : VIDEO_RENDERSWITCH_TEXTURE_STREAMING_MODE,
                     (char *)NULL);
    }

    m_overlayEnabled = enabled;
}


WId QGstreamerGLTextureRenderer::winId() const
{
    return m_winId;
}

void QGstreamerGLTextureRenderer::setWinId(WId id)
{
#ifdef GL_TEXTURE_SINK_DEBUG
    qDebug() << Q_FUNC_INFO << id;
#endif

    if (m_winId == id)
        return;

    bool oldReady = isReady();

    m_winId = id;

    if (m_videoSink && GST_IS_X_OVERLAY(m_videoSink)) {
        //don't set winId in NULL state,
        //texture sink opens xvideo port on set_xwindow_id,
        //this fails if video resource is not granted by resource policy yet.
        //state is changed to READY/PAUSED/PLAYING only after resource is granted.
        GstState pendingState = GST_STATE_NULL;
        GstState newState = GST_STATE_NULL;
        GstStateChangeReturn res = gst_element_get_state(m_videoSink,
                                                         &newState,
                                                         &pendingState,
                                                         0);//don't block and return immediately

        if (res != GST_STATE_CHANGE_FAILURE &&
                newState != GST_STATE_NULL &&
                pendingState != GST_STATE_NULL)
            gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(m_videoSink), m_winId);
    }

    if (oldReady != isReady())
        emit readyChanged(!oldReady);
}

QRect QGstreamerGLTextureRenderer::overlayGeometry() const
{
    return m_displayRect;
}

void QGstreamerGLTextureRenderer::setOverlayGeometry(const QRect &geometry)
{
    if (m_displayRect != geometry) {
#ifdef GL_TEXTURE_SINK_DEBUG
        qDebug() << Q_FUNC_INFO << geometry;
#endif
        m_displayRect = geometry;

        if (m_videoSink && GST_IS_X_OVERLAY(m_videoSink)) {
            if (m_displayRect.isEmpty())
                gst_x_overlay_set_render_rectangle(GST_X_OVERLAY(m_videoSink), -1, -1, -1, -1);
            else
                gst_x_overlay_set_render_rectangle(GST_X_OVERLAY(m_videoSink),
                                                   m_displayRect.x(),
                                                   m_displayRect.y(),
                                                   m_displayRect.width(),
                                                   m_displayRect.height());
            repaintOverlay();
        }
    }
}

QColor QGstreamerGLTextureRenderer::colorKey() const
{
    return m_colorKey;
}

void QGstreamerGLTextureRenderer::repaintOverlay()
{
    if (m_videoSink && GST_IS_X_OVERLAY(m_videoSink)) {
        //don't call gst_x_overlay_expose if the sink is in null state
        GstState state = GST_STATE_NULL;
        GstStateChangeReturn res = gst_element_get_state(m_videoSink, &state, NULL, 1000000);
        if (res != GST_STATE_CHANGE_FAILURE && state != GST_STATE_NULL) {
            gst_x_overlay_expose(GST_X_OVERLAY(m_videoSink));
        }
    }
}

QSize QGstreamerGLTextureRenderer::nativeSize() const
{
    return m_nativeSize;
}

gboolean QGstreamerGLTextureRenderer::padBufferProbe(GstPad *pad, GstBuffer *buffer, gpointer user_data)
{
    QGstreamerGLTextureRenderer *control = reinterpret_cast<QGstreamerGLTextureRenderer*>(user_data);
    QMetaObject::invokeMethod(control, "updateNativeVideoSize", Qt::QueuedConnection);
    gst_pad_remove_buffer_probe(pad, control->m_bufferProbeId);

    return TRUE;
}

void QGstreamerGLTextureRenderer::updateNativeVideoSize()
{
    const QSize oldSize = m_nativeSize;

    if (m_videoSink) {
        //find video native size to update video widget size hint
        GstPad *pad = gst_element_get_static_pad(m_videoSink,"sink");
        GstCaps *caps = gst_pad_get_negotiated_caps(pad);

        if (caps) {
            m_nativeSize = QGstUtils::capsCorrectedResolution(caps);
            gst_caps_unref(caps);
        }
    } else {
        m_nativeSize = QSize();
    }
#ifdef GL_TEXTURE_SINK_DEBUG
    qDebug() << Q_FUNC_INFO << oldSize << m_nativeSize << m_videoSink;
#endif

    if (m_nativeSize != oldSize)
        emit nativeSizeChanged();
}
