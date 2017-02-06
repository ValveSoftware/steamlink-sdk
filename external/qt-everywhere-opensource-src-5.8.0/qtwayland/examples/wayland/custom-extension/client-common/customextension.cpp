/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Wayland module
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "customextension.h"
#include <QtWaylandClient/private/qwaylanddisplay_p.h>
#include <QtWaylandClient/private/qwaylandintegration_p.h>
#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>
#include <QtGui/QPlatformSurfaceEvent>
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QDebug>

QT_BEGIN_NAMESPACE

CustomExtension::CustomExtension()
    : QWaylandClientExtensionTemplate(/* Supported protocol version */ 1 )
    , m_activated(false)
{
    connect(this, &CustomExtension::activeChanged, this, &CustomExtension::handleExtensionActive);
}


static inline struct ::wl_surface *getWlSurface(QWindow *window)
{
    void *surf = QGuiApplication::platformNativeInterface()->nativeResourceForWindow("surface", window);
    return static_cast<struct ::wl_surface *>(surf);
}

QWindow *CustomExtension::windowForSurface(struct ::wl_surface *surface)
{
    for (QWindow *w : qAsConst(m_windows)) {
        if (getWlSurface(w) == surface)
            return w;
    }
    return nullptr;
}

bool CustomExtension::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::PlatformSurface
            && static_cast<QPlatformSurfaceEvent*>(event)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated) {
        QWindow *window = qobject_cast<QWindow*>(object);
        Q_ASSERT(window);
        window->removeEventFilter(this);
        QtWayland::qt_example_extension::register_surface(getWlSurface(window));
    }
    return false;
}

void CustomExtension::sendWindowRegistration(QWindow *window)
{
    if (window->handle())
        QtWayland::qt_example_extension::register_surface(getWlSurface(window));
    else
        window->installEventFilter(this); // register when created
}

void CustomExtension::registerWindow(QWindow *window)
{
    m_windows << window;
    if (isActive())
        sendWindowRegistration(window);
}

void CustomExtension::sendBounce(QWindow *window, uint ms)
{
    QtWayland::qt_example_extension::bounce(getWlSurface(window), ms);
}

void CustomExtension::sendSpin(QWindow *window, uint ms)
{
    QtWayland::qt_example_extension::spin(getWlSurface(window), ms);
}

void CustomExtension::handleExtensionActive()
{
    if (isActive() && !m_activated) {
        for (QWindow *w : qAsConst(m_windows))
             sendWindowRegistration(w);
    }
}

void CustomExtension::example_extension_close(wl_surface *surface)
{
    QWindow *w = windowForSurface(surface);
    if (w)
        w->close();
}

void CustomExtension::example_extension_set_font_size(wl_surface *surface, uint32_t pixel_size)
{
    emit fontSize(windowForSurface(surface), pixel_size);
}

void CustomExtension::example_extension_set_window_decoration(uint32_t state)
{
    bool shown = state;
    for (QWindow *w : qAsConst(m_windows)) {
        Qt::WindowFlags f = w->flags();
        if (shown)
            f &= ~Qt::FramelessWindowHint;
        else
            f |= Qt::FramelessWindowHint;
        w->setFlags(f);
    }
}

QT_END_NAMESPACE
