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
#ifndef WINDOWGRABBER_H
#define WINDOWGRABBER_H

#define EGL_EGLEXT_PROTOTYPES = 1
#define GL_GLEXT_PROTOTYPES = 1
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/eglext.h>
#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QSize>
#include <QTimer>

#include <screen/screen.h>

QT_BEGIN_NAMESPACE

class WindowGrabberImage : public QObject
{
    Q_OBJECT

public:
    WindowGrabberImage();
    ~WindowGrabberImage();

    bool initialize(screen_context_t screenContext);

    void destroy();

    QImage getImage(screen_window_t window, const QSize &size);
    GLuint getTexture(screen_window_t window, const QSize &size);

private:
    bool grab(screen_window_t window);
    bool resize(const QSize &size);

    QSize m_size;
    screen_pixmap_t m_pixmap;
    screen_buffer_t m_pixmapBuffer;
    EGLImageKHR m_eglImage;
    GLuint m_glTexture;
    unsigned char *m_bufferAddress;
    int m_bufferStride;
};

class WindowGrabber : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit WindowGrabber(QObject *parent = 0);
    ~WindowGrabber();

    void setFrameRate(int frameRate);

    void setWindowId(const QByteArray &windowId);

    void start();
    void stop();

    void pause();
    void resume();

    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) Q_DECL_OVERRIDE;

    bool handleScreenEvent(screen_event_t event);

    QByteArray windowGroupId() const;

    bool eglImageSupported();
    void checkForEglImageExtension();

    int getNextTextureId();
    QImage getNextImage();

signals:
    void updateScene(const QSize &size);

private slots:
    void triggerUpdate();

private:
    bool selectBuffer();
    void cleanup();

    QTimer m_timer;

    QByteArray m_windowId;

    screen_window_t m_window;
    screen_context_t m_screenContext;

    WindowGrabberImage *m_images[2];
    QSize m_size;

    bool m_active;
    int m_currentFrame;
    bool m_eglImageSupported;
    bool m_eglImageCheck;       // We must not send a grabed frame before this is true
};

QT_END_NAMESPACE

#endif
