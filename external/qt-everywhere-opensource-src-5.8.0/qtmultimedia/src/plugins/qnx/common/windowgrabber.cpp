/****************************************************************************
**
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

#include "windowgrabber.h"

#include <QAbstractEventDispatcher>
#include <QDebug>
#include <QGuiApplication>
#include <QImage>
#include <QThread>
#include <qpa/qplatformnativeinterface.h>

#include <QOpenGLContext>
#include <QOpenGLFunctions>

#include <errno.h>

QT_BEGIN_NAMESPACE

static PFNEGLCREATEIMAGEKHRPROC s_eglCreateImageKHR;
static PFNEGLDESTROYIMAGEKHRPROC s_eglDestroyImageKHR;

WindowGrabber::WindowGrabber(QObject *parent)
    : QObject(parent),
      m_screenContext(0),
      m_active(false),
      m_currentFrame(0),
      m_eglImageSupported(false),
      m_eglImageCheck(false)
{
    // grab the window frame with 60 frames per second
    m_timer.setInterval(1000/60);

    connect(&m_timer, SIGNAL(timeout()), SLOT(triggerUpdate()));

    QCoreApplication::eventDispatcher()->installNativeEventFilter(this);

    for ( int i = 0; i < 2; ++i )
        m_images[i] = 0;

    // Use of EGL images can be disabled by setting QQNX_MM_DISABLE_EGLIMAGE_SUPPORT to something
    // non-zero.  This is probably useful only to test that this path still works since it results
    // in a high CPU load.
    if (!s_eglCreateImageKHR && qgetenv("QQNX_MM_DISABLE_EGLIMAGE_SUPPORT").toInt() == 0) {
        s_eglCreateImageKHR = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
        s_eglDestroyImageKHR = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
    }
}

WindowGrabber::~WindowGrabber()
{
    QCoreApplication::eventDispatcher()->removeNativeEventFilter(this);
    cleanup();
}

void WindowGrabber::setFrameRate(int frameRate)
{
    m_timer.setInterval(1000/frameRate);
}


void WindowGrabber::setWindowId(const QByteArray &windowId)
{
    m_windowId = windowId;
}

void WindowGrabber::start()
{
    if (m_active)
        return;

    if (!m_screenContext)
        screen_get_window_property_pv(m_window, SCREEN_PROPERTY_CONTEXT, reinterpret_cast<void**>(&m_screenContext));

    m_timer.start();

    m_active = true;
}

void WindowGrabber::stop()
{
    if (!m_active)
        return;

    cleanup();

    m_timer.stop();

    m_active = false;
}

void WindowGrabber::pause()
{
    m_timer.stop();
}

void WindowGrabber::resume()
{
    if (!m_active)
        return;

    m_timer.start();
}

bool WindowGrabber::handleScreenEvent(screen_event_t screen_event)
{

    int eventType;
    if (screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE, &eventType) != 0) {
        qWarning() << "WindowGrabber: Failed to query screen event type";
        return false;
    }

    if (eventType != SCREEN_EVENT_CREATE)
        return false;

    screen_window_t window = 0;
    if (screen_get_event_property_pv(screen_event, SCREEN_PROPERTY_WINDOW, (void**)&window) != 0) {
        qWarning() << "WindowGrabber: Failed to query window property";
        return false;
    }

    const int maxIdStrLength = 128;
    char idString[maxIdStrLength];
    if (screen_get_window_property_cv(window, SCREEN_PROPERTY_ID_STRING, maxIdStrLength, idString) != 0) {
        qWarning() << "WindowGrabber: Failed to query window ID string";
        return false;
    }

    // Grab windows that have a non-empty ID string and a matching window id to grab
    if (idString[0] != '\0' && m_windowId == idString) {
        m_window = window;
        start();
    }

    return false;
}

bool WindowGrabber::nativeEventFilter(const QByteArray &eventType, void *message, long*)
{
    if (eventType == "screen_event_t") {
        const screen_event_t event = static_cast<screen_event_t>(message);
        return handleScreenEvent(event);
    }

    return false;
}

QByteArray WindowGrabber::windowGroupId() const
{
    QWindow *window = QGuiApplication::allWindows().isEmpty() ? 0 : QGuiApplication::allWindows().first();
    if (!window)
        return QByteArray();

    QPlatformNativeInterface * const nativeInterface = QGuiApplication::platformNativeInterface();
    if (!nativeInterface) {
        qWarning() << "WindowGrabber: Unable to get platform native interface";
        return QByteArray();
    }

    const char * const groupIdData = static_cast<const char *>(
        nativeInterface->nativeResourceForWindow("windowGroup", window));
    if (!groupIdData) {
        qWarning() << "WindowGrabber: Unable to find window group for window" << window;
        return QByteArray();
    }

    return QByteArray(groupIdData);
}

bool WindowGrabber::eglImageSupported()
{
    return m_eglImageSupported;
}

void WindowGrabber::checkForEglImageExtension()
{
    QOpenGLContext *m_context = QOpenGLContext::currentContext();
    if (!m_context) //Should not happen, because we are called from the render thread
        return;

    QByteArray eglExtensions = QByteArray(eglQueryString(eglGetDisplay(EGL_DEFAULT_DISPLAY),
                                                         EGL_EXTENSIONS));
    m_eglImageSupported = m_context->hasExtension(QByteArrayLiteral("GL_OES_EGL_image"))
                          && eglExtensions.contains(QByteArrayLiteral("EGL_KHR_image"))
                          && s_eglCreateImageKHR && s_eglDestroyImageKHR;

    if (strstr(reinterpret_cast<const char*>(glGetString(GL_VENDOR)), "VMware"))
        m_eglImageSupported = false;

    m_eglImageCheck = true;
}

void WindowGrabber::triggerUpdate()
{
    if (!m_eglImageCheck) // We did not check for egl images yet
        return;

    int size[2] = { 0, 0 };

    int result = screen_get_window_property_iv(m_window, SCREEN_PROPERTY_SOURCE_SIZE, size);
    if (result != 0) {
        cleanup();
        qWarning() << "WindowGrabber: cannot get window size:" << strerror(errno);
        return;
    }

    if (m_size.width() != size[0] || m_size.height() != size[1])
        m_size = QSize(size[0], size[1]);

    emit updateScene(m_size);
}

bool WindowGrabber::selectBuffer()
{
    // If we're using egl images we need to double buffer since the gpu may still be using the last
    // video frame.  If we're not, it doesn't matter since the data is immediately copied.
    if (eglImageSupported())
        m_currentFrame = (m_currentFrame + 1) % 2;

    if (!m_images[m_currentFrame]) {
        m_images[m_currentFrame] = new WindowGrabberImage();
        if (!m_images[m_currentFrame]->initialize(m_screenContext)) {
            delete m_images[m_currentFrame];
            m_images[m_currentFrame] = 0;
            return false;
        }
    }
    return true;
}

int WindowGrabber::getNextTextureId()
{
    if (!selectBuffer())
        return 0;
    return m_images[m_currentFrame]->getTexture(m_window, m_size);
}

QImage WindowGrabber::getNextImage()
{
    if (!selectBuffer())
        return QImage();

    return m_images[m_currentFrame]->getImage(m_window, m_size);
}

void WindowGrabber::cleanup()
{
    for ( int i = 0; i < 2; ++i ) {
        if (m_images[i]) {
            m_images[i]->destroy();
            m_images[i] = 0;
        }
    }
}


WindowGrabberImage::WindowGrabberImage()
    : m_pixmap(0), m_pixmapBuffer(0), m_eglImage(0), m_glTexture(0), m_bufferAddress(0), m_bufferStride(0)
{
}

WindowGrabberImage::~WindowGrabberImage()
{
    if (m_glTexture)
        glDeleteTextures(1, &m_glTexture);
    if (m_eglImage)
        s_eglDestroyImageKHR(eglGetDisplay(EGL_DEFAULT_DISPLAY), m_eglImage);
    if (m_pixmap)
        screen_destroy_pixmap(m_pixmap);
}

bool
WindowGrabberImage::initialize(screen_context_t screenContext)
{
    if (screen_create_pixmap(&m_pixmap, screenContext) != 0) {
        qWarning() << "WindowGrabber: cannot create pixmap:" << strerror(errno);
        return false;
    }
    const int usage = SCREEN_USAGE_WRITE | SCREEN_USAGE_READ | SCREEN_USAGE_NATIVE;
    screen_set_pixmap_property_iv(m_pixmap, SCREEN_PROPERTY_USAGE, &usage);

    const int format = SCREEN_FORMAT_RGBX8888;
    screen_set_pixmap_property_iv(m_pixmap, SCREEN_PROPERTY_FORMAT, &format);

    return true;
}

void
WindowGrabberImage::destroy()
{
    // We want to delete in the thread we were created in since we need the thread that
    // has called eglMakeCurrent on the right EGL context.  This doesn't actually guarantee
    // this but that would be hard to achieve and in practice it works.
    if (QThread::currentThread() == thread())
        delete this;
    else
        deleteLater();
}

bool
WindowGrabberImage::resize(const QSize &newSize)
{
    if (m_pixmapBuffer) {
        screen_destroy_pixmap_buffer(m_pixmap);
        m_pixmapBuffer = 0;
        m_bufferAddress = 0;
        m_bufferStride = 0;
    }

    int size[2] = { newSize.width(), newSize.height() };

    screen_set_pixmap_property_iv(m_pixmap, SCREEN_PROPERTY_BUFFER_SIZE, size);

    if (screen_create_pixmap_buffer(m_pixmap) == 0) {
        screen_get_pixmap_property_pv(m_pixmap, SCREEN_PROPERTY_RENDER_BUFFERS,
                                      reinterpret_cast<void**>(&m_pixmapBuffer));
        screen_get_buffer_property_pv(m_pixmapBuffer, SCREEN_PROPERTY_POINTER,
                                      reinterpret_cast<void**>(&m_bufferAddress));
        screen_get_buffer_property_iv(m_pixmapBuffer, SCREEN_PROPERTY_STRIDE, &m_bufferStride);
        m_size = newSize;

        return true;
    } else {
        m_size = QSize();
        return false;
    }
}

bool
WindowGrabberImage::grab(screen_window_t window)
{
    const int rect[] = { 0, 0, m_size.width(), m_size.height() };
    return screen_read_window(window, m_pixmapBuffer, 1, rect, 0) == 0;
}

QImage
WindowGrabberImage::getImage(screen_window_t window, const QSize &size)
{
    if (size != m_size) {
        if (!resize(size))
            return QImage();
    }
    if (!m_bufferAddress || !grab(window))
        return QImage();

    return QImage(m_bufferAddress, m_size.width(), m_size.height(), m_bufferStride, QImage::Format_ARGB32);
}

GLuint
WindowGrabberImage::getTexture(screen_window_t window, const QSize &size)
{
    if (size != m_size) {
        if (!m_glTexture)
            glGenTextures(1, &m_glTexture);
        glBindTexture(GL_TEXTURE_2D, m_glTexture);
        if (m_eglImage) {
            glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, 0);
            s_eglDestroyImageKHR(eglGetDisplay(EGL_DEFAULT_DISPLAY), m_eglImage);
            m_eglImage = 0;
        }
        if (!resize(size))
            return 0;
        m_eglImage = s_eglCreateImageKHR(eglGetDisplay(EGL_DEFAULT_DISPLAY), EGL_NO_CONTEXT,
                                         EGL_NATIVE_PIXMAP_KHR, m_pixmap, 0);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, m_eglImage);
    }

    if (!m_pixmap || !grab(window))
        return 0;

    return m_glTexture;
}



QT_END_NAMESPACE
