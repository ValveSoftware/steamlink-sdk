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

#include "mmrendererplayervideorenderercontrol.h"

#include "windowgrabber.h"

#include <QCoreApplication>
#include <QDebug>
#include <QVideoSurfaceFormat>
#include <QOpenGLContext>

#include <mm/renderer.h>

QT_BEGIN_NAMESPACE

static int winIdCounter = 0;

MmRendererPlayerVideoRendererControl::MmRendererPlayerVideoRendererControl(QObject *parent)
    : QVideoRendererControl(parent)
    , m_windowGrabber(new WindowGrabber(this))
    , m_context(0)
    , m_videoId(-1)
{
    connect(m_windowGrabber, SIGNAL(updateScene(const QSize &)), SLOT(updateScene(const QSize &)));
}

MmRendererPlayerVideoRendererControl::~MmRendererPlayerVideoRendererControl()
{
    detachDisplay();
}

QAbstractVideoSurface *MmRendererPlayerVideoRendererControl::surface() const
{
    return m_surface;
}

void MmRendererPlayerVideoRendererControl::setSurface(QAbstractVideoSurface *surface)
{
    m_surface = QPointer<QAbstractVideoSurface>(surface);
    if (QOpenGLContext::currentContext())
        m_windowGrabber->checkForEglImageExtension();
    else if (m_surface)
        m_surface->setProperty("_q_GLThreadCallback", QVariant::fromValue<QObject*>(this));
}

void MmRendererPlayerVideoRendererControl::attachDisplay(mmr_context_t *context)
{
    if (m_videoId != -1) {
        qWarning() << "MmRendererPlayerVideoRendererControl: Video output already attached!";
        return;
    }

    if (!context) {
        qWarning() << "MmRendererPlayerVideoRendererControl: No media player context!";
        return;
    }

    const QByteArray windowGroupId = m_windowGrabber->windowGroupId();
    if (windowGroupId.isEmpty()) {
        qWarning() << "MmRendererPlayerVideoRendererControl: Unable to find window group";
        return;
    }

    const QString windowName = QStringLiteral("MmRendererPlayerVideoRendererControl_%1_%2")
                                             .arg(winIdCounter++)
                                             .arg(QCoreApplication::applicationPid());

    m_windowGrabber->setWindowId(windowName.toLatin1());

    // Start with an invisible window, because we just want to grab the frames from it.
    const QString videoDeviceUrl = QStringLiteral("screen:?winid=%1&wingrp=%2&initflags=invisible&nodstviewport=1")
                                                 .arg(windowName)
                                                 .arg(QString::fromLatin1(windowGroupId));

    m_videoId = mmr_output_attach(context, videoDeviceUrl.toLatin1(), "video");
    if (m_videoId == -1) {
        qWarning() << "mmr_output_attach() for video failed";
        return;
    }

    m_context = context;
}

void MmRendererPlayerVideoRendererControl::detachDisplay()
{
    m_windowGrabber->stop();

    if (m_surface)
        m_surface->stop();

    if (m_context && m_videoId != -1)
        mmr_output_detach(m_context, m_videoId);

    m_context = 0;
    m_videoId = -1;
}

void MmRendererPlayerVideoRendererControl::pause()
{
    m_windowGrabber->pause();
}

void MmRendererPlayerVideoRendererControl::resume()
{
    m_windowGrabber->resume();
}

class QnxTextureBuffer : public QAbstractVideoBuffer
{
public:
    QnxTextureBuffer(WindowGrabber *windowGrabber) :
        QAbstractVideoBuffer(QAbstractVideoBuffer::GLTextureHandle)
    {
        m_windowGrabber = windowGrabber;
        m_handle = 0;
    }
    MapMode mapMode() const {
        return QAbstractVideoBuffer::ReadWrite;
    }
    void unmap() {

    }
    uchar *map(MapMode mode, int * numBytes, int * bytesPerLine) {
        Q_UNUSED(mode);
        Q_UNUSED(numBytes);
        Q_UNUSED(bytesPerLine);
        return 0;
    }
    QVariant handle() const {
        if (!m_handle) {
            const_cast<QnxTextureBuffer*>(this)->m_handle = m_windowGrabber->getNextTextureId();
        }
        return m_handle;
    }
private:
    WindowGrabber *m_windowGrabber;
    int m_handle;
};

void MmRendererPlayerVideoRendererControl::updateScene(const QSize &size)
{
    if (m_surface) {
        if (!m_surface->isActive()) {
            if (m_windowGrabber->eglImageSupported()) {
                m_surface->start(QVideoSurfaceFormat(size, QVideoFrame::Format_BGR32,
                                                     QAbstractVideoBuffer::GLTextureHandle));
            } else {
                m_surface->start(QVideoSurfaceFormat(size, QVideoFrame::Format_ARGB32));
            }
        } else {
            if (m_surface->surfaceFormat().frameSize() != size) {
                m_surface->stop();
                if (m_windowGrabber->eglImageSupported()) {
                    m_surface->start(QVideoSurfaceFormat(size, QVideoFrame::Format_BGR32,
                                                         QAbstractVideoBuffer::GLTextureHandle));
                } else {
                    m_surface->start(QVideoSurfaceFormat(size, QVideoFrame::Format_ARGB32));
                }
            }
        }

        // Depending on the support of EGL images on the current platform we either pass a texture
        // handle or a copy of the image data
        if (m_windowGrabber->eglImageSupported()) {
            QnxTextureBuffer *textBuffer = new QnxTextureBuffer(m_windowGrabber);
            QVideoFrame actualFrame(textBuffer, size, QVideoFrame::Format_BGR32);
            m_surface->present(actualFrame);
        } else {
            m_surface->present(m_windowGrabber->getNextImage().copy());
        }
    }
}

void MmRendererPlayerVideoRendererControl::customEvent(QEvent *e)
{
    // This is running in the render thread (OpenGL enabled)
    if (e->type() == QEvent::User)
        m_windowGrabber->checkForEglImageExtension();
}

QT_END_NAMESPACE
