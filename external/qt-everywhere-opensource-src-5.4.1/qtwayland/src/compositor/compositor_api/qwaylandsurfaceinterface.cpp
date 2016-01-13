/****************************************************************************
**
** Copyright (C) 2014 Jolla Ltd, author: <giulio.camuffo@jollamobile.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Compositor.
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

#include <wayland-server.h>

#include "qwaylandsurfaceinterface.h"
#include "qwaylandsurface.h"
#include "qwaylandsurface_p.h"

class QWaylandSurfaceInterface::Private
{
public:
    QWaylandSurface *surface;
};

QWaylandSurfaceInterface::QWaylandSurfaceInterface(QWaylandSurface *surface)
                   : d(new Private)
{
    d->surface = surface;
    surface->addInterface(this);
}

QWaylandSurfaceInterface::~QWaylandSurfaceInterface()
{
    d->surface->removeInterface(this);
}

QWaylandSurface *QWaylandSurfaceInterface::surface() const
{
    return d->surface;
}

void QWaylandSurfaceInterface::setSurfaceType(QWaylandSurface::WindowType type)
{
    surface()->d_func()->setType(type);
}

void QWaylandSurfaceInterface::setSurfaceClassName(const QString &name)
{
    surface()->d_func()->setClassName(name);
}

void QWaylandSurfaceInterface::setSurfaceTitle(const QString &title)
{
    surface()->d_func()->setTitle(title);
}



class QWaylandSurfaceOp::Private
{
public:
    int type;
};

QWaylandSurfaceOp::QWaylandSurfaceOp(int t)
                 : d(new Private)
{
    d->type = t;
}

int QWaylandSurfaceOp::type() const
{
    return d->type;
}



QWaylandSurfaceSetVisibilityOp::QWaylandSurfaceSetVisibilityOp(QWindow::Visibility visibility)
                              : QWaylandSurfaceOp(Type::SetVisibility)
                              , m_visibility(visibility)
{
}

QWindow::Visibility QWaylandSurfaceSetVisibilityOp::visibility() const
{
    return m_visibility;
}

QWaylandSurfaceResizeOp::QWaylandSurfaceResizeOp(const QSize &size)
                       : QWaylandSurfaceOp(Type::Resize)
                       , m_size(size)
{
}

QSize QWaylandSurfaceResizeOp::size() const
{
    return m_size;
}

QWaylandSurfacePingOp::QWaylandSurfacePingOp(uint32_t serial)
                     : QWaylandSurfaceOp(Type::Ping)
                     , m_serial(serial)
{
}

uint32_t QWaylandSurfacePingOp::serial() const
{
    return m_serial;
}
