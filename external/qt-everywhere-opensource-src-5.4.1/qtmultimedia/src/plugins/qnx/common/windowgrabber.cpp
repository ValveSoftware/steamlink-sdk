/****************************************************************************
**
** Copyright (C) 2013 Research In Motion
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

#include "windowgrabber.h"

#include <QAbstractEventDispatcher>
#include <QDebug>
#include <QGuiApplication>
#include <QImage>
#include <qpa/qplatformnativeinterface.h>

#include <QOpenGLContext>

#ifdef Q_OS_BLACKBERRY
#include <bps/event.h>
#include <bps/screen.h>
#endif
#include <errno.h>

QT_BEGIN_NAMESPACE

WindowGrabber::WindowGrabber(QObject *parent)
    : QObject(parent),
      m_screenBufferWidth(-1),
      m_screenBufferHeight(-1),
      m_active(false),
      m_screenContextInitialized(false),
      m_screenPixmapBuffersInitialized(false),
      m_currentFrame(0),
      m_eglImageSupported(false),
      m_eglImagesInitialized(false),
      m_eglImageCheck(false)
{
    // grab the window frame with 60 frames per second
    m_timer.setInterval(1000/60);

    connect(&m_timer, SIGNAL(timeout()), SLOT(grab()));

    QCoreApplication::eventDispatcher()->installNativeEventFilter(this);
}

WindowGrabber::~WindowGrabber()
{
    QCoreApplication::eventDispatcher()->removeNativeEventFilter(this);
    if (eglImagesInitialized()) {
        glDeleteTextures(2, imgTextures);
        eglDestroyImageKHR(eglGetDisplay(EGL_DEFAULT_DISPLAY), img[0]);
        eglDestroyImageKHR(eglGetDisplay(EGL_DEFAULT_DISPLAY), img[1]);
    }
}

void WindowGrabber::setFrameRate(int frameRate)
{
    m_timer.setInterval(1000/frameRate);
}

void WindowGrabber::createEglImages()
{
    // Do nothing if either egl images are not supported, the screen context is not valid
    // or the images are already created
    if (!eglImageSupported() || !m_screenContextInitialized || eglImagesInitialized())
        return;

    glGenTextures(2, imgTextures);
    glBindTexture(GL_TEXTURE_2D, imgTextures[0]);
    img[0] = eglCreateImageKHR(eglGetDisplay(EGL_DEFAULT_DISPLAY), EGL_NO_CONTEXT,
                                        EGL_NATIVE_PIXMAP_KHR,
                                        m_screenPixmaps[0],
                                        0);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, img[0]);

    glBindTexture(GL_TEXTURE_2D, imgTextures[1]);
    img[1] = eglCreateImageKHR(eglGetDisplay(EGL_DEFAULT_DISPLAY), EGL_NO_CONTEXT,
                                        EGL_NATIVE_PIXMAP_KHR,
                                        m_screenPixmaps[1],
                                        0);

    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, img[1]);

    if (img[0] == 0 || img[1] == 0) {
        qWarning() << "Failed to create KHR images" << img[0] << img[1] << strerror(errno) << errno;
        m_eglImageSupported = false;
    } else {
        m_eglImagesInitialized = true;
    }
}

void WindowGrabber::setWindowId(const QByteArray &windowId)
{
    m_windowId = windowId;
}

void WindowGrabber::start()
{
    if (m_active)
        return;

    int result = 0;

#ifdef Q_OS_BLACKBERRY_TABLET

    // HACK: On the Playbook, screen_read_window() will fail for invisible windows.
    //       To workaround this, make the window visible again, but set a global
    //       alpha of less than 255. The global alpha makes the window completely invisible
    //       (due to a bug?), but screen_read_window() will work again.

    errno = 0;
    int val = 200; // anything less than 255
    result = screen_set_window_property_iv(m_window, SCREEN_PROPERTY_GLOBAL_ALPHA, &val);
    if (result != 0) {
        qWarning() << "WindowGrabber: unable to set global alpha:" << strerror(errno);
        return;
    }

    errno = 0;
    val = 1;
    result = screen_set_window_property_iv(m_window, SCREEN_PROPERTY_VISIBLE, &val);
    if (result != 0) {
        qWarning() << "WindowGrabber: unable to make window visible:" << strerror(errno);
        return;
    }
#endif

    result = screen_create_context(&m_screenContext, SCREEN_APPLICATION_CONTEXT);
    if (result != 0) {
        qWarning() << "WindowGrabber: cannot create screen context:" << strerror(errno);
        return;
    } else {
        m_screenContextInitialized = true;
    }

    result = screen_create_pixmap(&m_screenPixmaps[0], m_screenContext);
    result = screen_create_pixmap(&m_screenPixmaps[1], m_screenContext);
    if (result != 0) {
        cleanup();
        qWarning() << "WindowGrabber: cannot create pixmaps:" << strerror(errno);
        return;
    }

    const int usage = SCREEN_USAGE_NATIVE;
    result = screen_set_pixmap_property_iv(m_screenPixmaps[0], SCREEN_PROPERTY_USAGE, &usage);
    result |= screen_set_pixmap_property_iv(m_screenPixmaps[1], SCREEN_PROPERTY_USAGE, &usage);

    const int format = SCREEN_FORMAT_RGBX8888;
    screen_set_pixmap_property_iv(m_screenPixmaps[0], SCREEN_PROPERTY_FORMAT, &format);
    screen_set_pixmap_property_iv(m_screenPixmaps[1], SCREEN_PROPERTY_FORMAT, &format);

    int size[2] = { 0, 0 };

    result = screen_get_window_property_iv(m_window, SCREEN_PROPERTY_SOURCE_SIZE, size);
    if (result != 0) {
        cleanup();
        qWarning() << "WindowGrabber: cannot get window size:" << strerror(errno);
        return;
    }

    m_screenBufferWidth = size[0];
    m_screenBufferHeight = size[1];

    updateFrameSize();

    m_timer.start();

    m_active = true;
}

void WindowGrabber::updateFrameSize()
{
    int size[2] = { m_screenBufferWidth, m_screenBufferHeight };

    screen_set_pixmap_property_iv(m_screenPixmaps[0], SCREEN_PROPERTY_BUFFER_SIZE, size);
    if (eglImageSupported())
        screen_set_pixmap_property_iv(m_screenPixmaps[1], SCREEN_PROPERTY_BUFFER_SIZE, size);

    int result = screen_create_pixmap_buffer(m_screenPixmaps[0]);
    if (eglImageSupported())
        result |= screen_create_pixmap_buffer(m_screenPixmaps[1]);

    if (result != 0) {
        cleanup();
        qWarning() << "WindowGrabber: cannot create pixmap buffer:" << strerror(errno);
        return;
    } else {
        m_screenPixmapBuffersInitialized = true;
    }

    result = screen_get_pixmap_property_pv(m_screenPixmaps[0], SCREEN_PROPERTY_RENDER_BUFFERS,
            (void**)&m_screenPixmapBuffers[0]);
    if (eglImageSupported()) {
        result |= screen_get_pixmap_property_pv(m_screenPixmaps[1], SCREEN_PROPERTY_RENDER_BUFFERS,
                (void**)&m_screenPixmapBuffers[1]);
    }

    if (result != 0) {
        cleanup();
        qWarning() << "WindowGrabber: cannot get pixmap buffer:" << strerror(errno);
        return;
    }

    result = screen_get_buffer_property_pv(m_screenPixmapBuffers[0], SCREEN_PROPERTY_POINTER,
            (void**)&m_screenBuffers[0]);
    if (eglImageSupported()) {
        result |= screen_get_buffer_property_pv(m_screenPixmapBuffers[1], SCREEN_PROPERTY_POINTER,
                (void**)&m_screenBuffers[1]);
    }

    if (result != 0) {
        cleanup();
        qWarning() << "WindowGrabber: cannot get pixmap buffer pointer:" << strerror(errno);
        return;
    }

    result = screen_get_buffer_property_iv(m_screenPixmapBuffers[0], SCREEN_PROPERTY_STRIDE,
            &m_screenBufferStride);

    if (result != 0) {
        cleanup();
        qWarning() << "WindowGrabber: cannot get pixmap buffer stride:" << strerror(errno);
        return;
    }
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

    if (m_windowId == idString) {
        m_window = window;
        start();
    }

    return false;
}

bool WindowGrabber::nativeEventFilter(const QByteArray &eventType, void *message, long*)
{
#ifdef Q_OS_BLACKBERRY
    Q_UNUSED(eventType)
    bps_event_t * const event = static_cast<bps_event_t *>(message);

    if (event && bps_event_get_domain(event) == screen_get_domain()) {
        const screen_event_t screen_event = screen_event_get_event(event);
        return handleScreenEvent(screen_event);
    }
#else
    if (eventType == "screen_event_t") {
        const screen_event_t event = static_cast<screen_event_t>(message);
        return handleScreenEvent(event);
    }
#endif

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
                          && eglExtensions.contains(QByteArrayLiteral("EGL_KHR_image"));

    m_eglImageCheck = true;
}

bool WindowGrabber::eglImagesInitialized()
{
    return m_eglImagesInitialized;
}

void WindowGrabber::grab()
{
    if (!m_eglImageCheck) // We did not check for egl images yet
        return;

    if (eglImageSupported())
        m_currentFrame = (m_currentFrame + 1) % 2;
    else
        m_currentFrame = 0;

    int size[2] = { 0, 0 };

    int result = screen_get_window_property_iv(m_window, SCREEN_PROPERTY_SOURCE_SIZE, size);
    if (result != 0) {
        cleanup();
        qWarning() << "WindowGrabber: cannot get window size:" << strerror(errno);
        return;
    }

    if (m_screenBufferWidth != size[0] || m_screenBufferHeight != size[1]) {
        // The source viewport size changed, so we have to adapt our buffers

        if (m_screenPixmapBuffersInitialized) {
            screen_destroy_pixmap_buffer(m_screenPixmaps[0]);
            if (eglImageSupported())
                screen_destroy_pixmap_buffer(m_screenPixmaps[1]);
        }

        m_screenBufferWidth = size[0];
        m_screenBufferHeight = size[1];

        updateFrameSize();
        m_eglImagesInitialized = false;
    }

    const int rect[] = { 0, 0, m_screenBufferWidth, m_screenBufferHeight };
    result = screen_read_window(m_window, m_screenPixmapBuffers[m_currentFrame], 1, rect, 0);
    if (result != 0)
        return;

    const QImage frame((unsigned char*)m_screenBuffers[m_currentFrame], m_screenBufferWidth,
                       m_screenBufferHeight, m_screenBufferStride, QImage::Format_ARGB32);

    emit frameGrabbed(frame, imgTextures[m_currentFrame]);
}

void WindowGrabber::cleanup()
{
    //We only need to destroy the context as it frees all resources associated with it
    if (m_screenContextInitialized) {
        screen_destroy_context(m_screenContext);
        m_screenContextInitialized = false;
    }
}

QT_END_NAMESPACE
