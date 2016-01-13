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

#include "mockcompositor.h"
#include "mocksurface.h"

namespace Impl {

void shell_surface_pong(wl_client *client,
                        wl_resource *surface_resource,
                        uint32_t serial)
{
    Q_UNUSED(client);
    Q_UNUSED(surface_resource);
    Q_UNUSED(serial);
}

void shell_surface_move(wl_client *client,
                        wl_resource *surface_resource,
                        wl_resource *input_device_resource,
                        uint32_t time)
{
    Q_UNUSED(client);
    Q_UNUSED(surface_resource);
    Q_UNUSED(input_device_resource);
    Q_UNUSED(time);
}

void shell_surface_resize(wl_client *client,
                          wl_resource *surface_resource,
                          wl_resource *input_device_resource,
                          uint32_t time,
                          uint32_t edges)
{
    Q_UNUSED(client);
    Q_UNUSED(surface_resource);
    Q_UNUSED(input_device_resource);
    Q_UNUSED(time);
    Q_UNUSED(edges);

}

void shell_surface_set_toplevel(wl_client *client,
                                wl_resource *surface_resource)
{
    Q_UNUSED(client);
    Q_UNUSED(surface_resource);
}

void shell_surface_set_transient(wl_client *client,
                                 wl_resource *surface_resource,
                                 wl_resource *parent_surface_resource,
                                 int x,
                                 int y,
                                 uint32_t flags)
{

    Q_UNUSED(client);
    Q_UNUSED(surface_resource);
    Q_UNUSED(parent_surface_resource);
    Q_UNUSED(x);
    Q_UNUSED(y);
    Q_UNUSED(flags);
}

void shell_surface_set_fullscreen(wl_client *client,
                                  wl_resource *surface_resource,
                                  uint32_t method,
                                  uint32_t framerate,
                                  wl_resource *output)
{
    Q_UNUSED(client);
    Q_UNUSED(surface_resource);
    Q_UNUSED(method);
    Q_UNUSED(framerate);
    Q_UNUSED(output);
}

void shell_surface_set_popup(wl_client *client, wl_resource *resource,
                             wl_resource *input_device, uint32_t time,
                             wl_resource *parent,
                             int32_t x, int32_t y,
                             uint32_t flags)
{
    Q_UNUSED(client);
    Q_UNUSED(resource);
    Q_UNUSED(input_device);
    Q_UNUSED(time);
    Q_UNUSED(parent);
    Q_UNUSED(x);
    Q_UNUSED(y);
    Q_UNUSED(flags);
}

void shell_surface_set_maximized(wl_client *client,
                                 wl_resource *surface_resource,
                                 wl_resource *output)
{
    Q_UNUSED(client);
    Q_UNUSED(surface_resource);
    Q_UNUSED(output);
}

void shell_surface_set_title(wl_client *client,
                             wl_resource *surface_resource,
                             const char *title)
{
    Q_UNUSED(client);
    Q_UNUSED(surface_resource);
    Q_UNUSED(title);
}

void shell_surface_set_class(wl_client *client,
                             wl_resource *surface_resource,
                             const char *class_)
{
    Q_UNUSED(client);
    Q_UNUSED(surface_resource);
    Q_UNUSED(class_);
}

static void get_shell_surface(wl_client *client, wl_resource *compositorResource, uint32_t id, wl_resource *surfaceResource)
{
    static const struct wl_shell_surface_interface shellSurfaceInterface = {
        shell_surface_pong,
        shell_surface_move,
        shell_surface_resize,
        shell_surface_set_toplevel,
        shell_surface_set_transient,
        shell_surface_set_fullscreen,
        shell_surface_set_popup,
        shell_surface_set_maximized,
        shell_surface_set_title,
        shell_surface_set_class
    };

    Q_UNUSED(compositorResource);
    wl_client_add_object(client, &wl_shell_surface_interface, &shellSurfaceInterface, id, surfaceResource->data);
    Surface *surf = Surface::fromResource(surfaceResource);
    surf->map();
}

void Compositor::bindShell(wl_client *client, void *compositorData, uint32_t version, uint32_t id)
{
    static const struct wl_shell_interface shellInterface = {
        get_shell_surface
    };

    Q_UNUSED(version);
    wl_client_add_object(client, &wl_shell_interface, &shellInterface, id, compositorData);
}

}

