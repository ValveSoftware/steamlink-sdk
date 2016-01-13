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
#include <QTimer>

#include <screen/screen.h>

QT_BEGIN_NAMESPACE

class WindowGrabber : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit WindowGrabber(QObject *parent = 0);
    ~WindowGrabber();

    void setFrameRate(int frameRate);

    void createEglImages();

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
    bool eglImagesInitialized();

signals:
    void frameGrabbed(const QImage &frame, int);

private slots:
    void grab();

private:
    void cleanup();
    void updateFrameSize();

    QTimer m_timer;

    QByteArray m_windowId;

    screen_window_t m_window;
    screen_context_t m_screenContext;
    screen_pixmap_t m_screenPixmaps[2];
    screen_buffer_t m_screenPixmapBuffers[2];

    char *m_screenBuffers[2];

    int m_screenBufferWidth;
    int m_screenBufferHeight;
    int m_screenBufferStride;

    bool m_active : 1;
    bool m_screenContextInitialized : 1;
    bool m_screenPixmapBuffersInitialized : 1;
    int m_currentFrame;
    EGLImageKHR img[2];
    GLuint imgTextures[2];
    bool m_eglImageSupported : 1;
    bool m_eglImagesInitialized : 1;
    bool m_eglImageCheck : 1; // We must not send a grabed frame before this is true
};

QT_END_NAMESPACE

#endif
