/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt-project.org/legal
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

#include <QWaylandSurface>

#include <QDebug>

CustomExtension::CustomExtension(QWaylandCompositor *compositor)
    :QWaylandCompositorExtensionTemplate(compositor)
{
}

void CustomExtension::initialize()
{
    QWaylandCompositorExtensionTemplate::initialize();
    QWaylandCompositor *compositor = static_cast<QWaylandCompositor *>(extensionContainer());
    init(compositor->display(), 1);
}

void CustomExtension::setFontSize(QWaylandSurface *surface, uint pixelSize)
{
    if (surface) {
        Resource *target = resourceMap().value(surface->waylandClient());
        if (target) {
            qDebug() << "Server-side extension sending setFontSize:" << pixelSize;
            send_set_font_size(target->handle,  surface->resource(), pixelSize);
        }
    }
}

void CustomExtension::showDecorations(QWaylandClient *client, bool shown)
{
    if (client) {
        Resource *target = resourceMap().value(client->client());
        if (target) {
            qDebug() << "Server-side extension sending showDecorations:" << shown;
            send_set_window_decoration(target->handle, shown);
        }
    }

}

void CustomExtension::close(QWaylandSurface *surface)
{
    if (surface) {
        Resource *target = resourceMap().value(surface->waylandClient());
        if (target) {
            qDebug() << "Server-side extension sending close for" << surface;
            send_close(target->handle,  surface->resource());
        }
    }
}

void CustomExtension::example_extension_bounce(QtWaylandServer::qt_example_extension::Resource *resource, wl_resource *wl_surface, uint32_t duration)
{
    Q_UNUSED(resource);
    auto surface = QWaylandSurface::fromResource(wl_surface);
    qDebug() << "server received bounce" << surface << duration;
    emit bounce(surface, duration);
}

void CustomExtension::example_extension_spin(QtWaylandServer::qt_example_extension::Resource *resource, wl_resource *wl_surface, uint32_t duration)
{
    Q_UNUSED(resource);
    auto surface = QWaylandSurface::fromResource(wl_surface);
    qDebug() << "server received spin" << surface << duration;
    emit spin(surface, duration);
}

void CustomExtension::example_extension_register_surface(QtWaylandServer::qt_example_extension::Resource *resource, wl_resource *wl_surface)
{
    Q_UNUSED(resource);
    auto surface = QWaylandSurface::fromResource(wl_surface);
    qDebug() << "server received new surface" << surface;
    emit surfaceAdded(surface);
}
