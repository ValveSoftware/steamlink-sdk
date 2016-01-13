/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#include "qwlsubsurface_p.h"

#include "qwlcompositor_p.h"
#include "qwaylandsurface.h"
#include "qwaylandsurfaceview.h"

QT_BEGIN_NAMESPACE

namespace QtWayland {

SubSurfaceExtensionGlobal::SubSurfaceExtensionGlobal(Compositor *compositor)
    : m_compositor(compositor)
{
    wl_global_create(m_compositor->wl_display(),
                     &qt_sub_surface_extension_interface,
                     qt_sub_surface_extension_interface.version,
                     this,
                     SubSurfaceExtensionGlobal::bind_func);
}

void SubSurfaceExtensionGlobal::bind_func(wl_client *client, void *data, uint32_t version, uint32_t id)
{
    Q_UNUSED(version);
    struct wl_resource *resource = wl_resource_create(client, &qt_sub_surface_extension_interface,version,id);
    wl_resource_set_implementation(resource, &sub_surface_extension_interface, data, 0);
}

void SubSurfaceExtensionGlobal::get_sub_surface_aware_surface(wl_client *client, wl_resource *sub_surface_extension_resource, uint32_t id, wl_resource *surface_resource)
{
    Q_UNUSED(sub_surface_extension_resource);
    Surface *surface = Surface::fromResource(surface_resource);
    new SubSurface(client,id,surface);
}

const struct qt_sub_surface_extension_interface SubSurfaceExtensionGlobal::sub_surface_extension_interface = {
    SubSurfaceExtensionGlobal::get_sub_surface_aware_surface
};

SubSurface::SubSurface(wl_client *client, uint32_t id, Surface *surface)
    : m_surface(surface)
    , m_parent(0)
{
    surface->setSubSurface(this);
    m_sub_surface_resource = wl_resource_create(client, &qt_sub_surface_interface, qt_sub_surface_interface.version, id);
    wl_resource_set_implementation(m_sub_surface_resource, &sub_surface_interface, this, 0);
}

SubSurface::~SubSurface()
{
    if (m_parent) {
        m_parent->removeSubSurface(this);
    }
    QLinkedList<QWaylandSurface *>::iterator it;
    for (it = m_sub_surfaces.begin(); it != m_sub_surfaces.end(); ++it) {
        (*it)->handle()->subSurface()->parentDestroyed();
    }
}

void SubSurface::setSubSurface(SubSurface *subSurface, int x, int y)
{
    if (!m_sub_surfaces.contains(subSurface->m_surface->waylandSurface())) {
        m_sub_surfaces.append(subSurface->m_surface->waylandSurface());
        subSurface->setParent(this);
    }
    foreach (QWaylandSurfaceView *view, subSurface->m_surface->waylandSurface()->views())
        view->setPos(QPointF(x,y));
}

void SubSurface::removeSubSurface(SubSurface *subSurfaces)
{
    Q_ASSERT(m_sub_surfaces.contains(subSurfaces->m_surface->waylandSurface()));
    m_sub_surfaces.removeOne(subSurfaces->m_surface->waylandSurface());
}

SubSurface *SubSurface::parent() const
{
    return m_parent;
}

void SubSurface::setParent(SubSurface *parent)
{
    if (m_parent == parent)
        return;

    QWaylandSurface *oldParent = 0;
    QWaylandSurface *newParent = 0;

    if (m_parent) {
        oldParent = m_parent->m_surface->waylandSurface();
        m_parent->removeSubSurface(this);
    }
    if (parent) {
        newParent = parent->m_surface->waylandSurface();
    }
    m_parent = parent;

    m_surface->waylandSurface()->parentChanged(newParent,oldParent);
}

QLinkedList<QWaylandSurface *> SubSurface::subSurfaces() const
{
    return m_sub_surfaces;
}

void SubSurface::parentDestroyed()
{
    m_parent = 0;
}
void SubSurface::attach_sub_surface(wl_client *client, wl_resource *sub_surface_parent_resource, wl_resource *sub_surface_child_resource, int32_t x, int32_t y)
{
    Q_UNUSED(client);
    SubSurface *parent_sub_surface = static_cast<SubSurface *>(sub_surface_parent_resource->data);
    SubSurface *child_sub_surface = static_cast<SubSurface *>(sub_surface_child_resource->data);
    parent_sub_surface->setSubSurface(child_sub_surface,x,y);
}

void SubSurface::move_sub_surface(wl_client *client, wl_resource *sub_surface_parent_resource, wl_resource *sub_surface_child_resource, int32_t x, int32_t y)
{
    Q_UNUSED(client);
    Q_UNUSED(x);
    Q_UNUSED(y);
    SubSurface *parent_sub_surface = static_cast<SubSurface *>(sub_surface_parent_resource->data);
    SubSurface *child_sub_surface = static_cast<SubSurface *>(sub_surface_child_resource->data);
    Q_UNUSED(parent_sub_surface);
    Q_UNUSED(child_sub_surface);
}

void SubSurface::raise(wl_client *client, wl_resource *sub_surface_parent_resource, wl_resource *sub_surface_child_resource)
{
    Q_UNUSED(client);
    Q_UNUSED(sub_surface_parent_resource);
    Q_UNUSED(sub_surface_child_resource);
}

void SubSurface::lower(wl_client *client, wl_resource *sub_surface_parent_resource, wl_resource *sub_surface_child_resource)
{
    Q_UNUSED(client);
    Q_UNUSED(sub_surface_parent_resource);
    Q_UNUSED(sub_surface_child_resource);
}

const struct qt_sub_surface_interface SubSurface::sub_surface_interface = {
    SubSurface::attach_sub_surface,
    SubSurface::move_sub_surface,
    SubSurface::raise,
    SubSurface::lower
};

}

QT_END_NAMESPACE
