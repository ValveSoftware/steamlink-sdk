/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "mocksurface.h"
#include "mockcompositor.h"

namespace Impl {

Surface::Surface(wl_client *client, uint32_t id, int v, Compositor *compositor)
    : QtWaylandServer::wl_surface(client, id, v)
    , m_buffer(Q_NULLPTR)
    , m_compositor(compositor)
    , m_mockSurface(new MockSurface(this))
    , m_mapped(false)
{
}

Surface::~Surface()
{
    m_mockSurface->m_surface = 0;
}

void Surface::map()
{
    m_mapped = true;
}

bool Surface::isMapped() const
{
    return m_mapped;
}

Surface *Surface::fromResource(struct ::wl_resource *resource)
{
    return static_cast<Surface *>(Resource::fromResource(resource)->surface_object);
}

void Surface::surface_destroy_resource(Resource *)
{
    compositor()->removeSurface(this);
    delete this;
}

void Surface::surface_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void Surface::surface_attach(Resource *resource,
                             struct wl_resource *buffer, int x, int y)
{
    Q_UNUSED(resource);
    Q_UNUSED(x);
    Q_UNUSED(y);
    m_buffer = buffer;

    if (!buffer)
        m_mockSurface->image = QImage();
}

void Surface::surface_damage(Resource *resource,
                             int32_t x, int32_t y, int32_t width, int32_t height)
{
    Q_UNUSED(resource);
    Q_UNUSED(x);
    Q_UNUSED(y);
    Q_UNUSED(width);
    Q_UNUSED(height);
}

void Surface::surface_frame(Resource *resource,
                            uint32_t callback)
{
    wl_resource *frameCallback = wl_client_add_object(resource->client(), &wl_callback_interface, 0, callback, this);
    m_frameCallbackList << frameCallback;
}

void Surface::surface_commit(Resource *resource)
{
    Q_UNUSED(resource);

    if (m_buffer) {
#if WAYLAND_VERSION_CHECK(1, 2, 0)
        struct ::wl_shm_buffer *shm_buffer = wl_shm_buffer_get(m_buffer);
#else
        struct ::wl_buffer *shm_buffer = 0;
        if (wl_buffer_is_shm(static_cast<struct ::wl_buffer*>(m_buffer->data)))
            shm_buffer = static_cast<struct ::wl_buffer*>(m_buffer->data);
#endif

        if (shm_buffer) {
            int stride = wl_shm_buffer_get_stride(shm_buffer);
            uint format = wl_shm_buffer_get_format(shm_buffer);
            Q_UNUSED(format);
            void *data = wl_shm_buffer_get_data(shm_buffer);
            const uchar *char_data = static_cast<const uchar *>(data);
            QImage img(char_data, wl_shm_buffer_get_width(shm_buffer), wl_shm_buffer_get_height(shm_buffer), stride, QImage::Format_ARGB32_Premultiplied);
            m_mockSurface->image = img;
        }
    }

    foreach (wl_resource *frameCallback, m_frameCallbackList) {
        wl_callback_send_done(frameCallback, m_compositor->time());
        wl_resource_destroy(frameCallback);
    }
    m_frameCallbackList.clear();
}

}
MockSurface::MockSurface(Impl::Surface *surface)
    : m_surface(surface)
{
}
