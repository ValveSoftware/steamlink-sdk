/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwldatasource_p.h"
#include "qwldataoffer_p.h"
#include "qwldatadevice_p.h"
#include "qwldatadevicemanager_p.h"

#include <unistd.h>
#include <QtWaylandCompositor/private/wayland-wayland-server-protocol.h>

QT_BEGIN_NAMESPACE

namespace QtWayland {

DataSource::DataSource(struct wl_client *client, uint32_t id, uint32_t time)
    : QtWaylandServer::wl_data_source(client, id, 1)
    , m_time(time)
    , m_device(0)
    , m_manager(0)
{
}

DataSource::~DataSource()
{
    if (m_manager)
        m_manager->sourceDestroyed(this);
    if (m_device)
        m_device->sourceDestroyed(this);
}

uint32_t DataSource::time() const
{
    return m_time;
}

QList<QString> DataSource::mimeTypes() const
{
    return m_mimeTypes;
}

void DataSource::accept(const QString &mimeType)
{
    send_target(mimeType);
}

void DataSource::send(const QString &mimeType, int fd)
{
    send_send(mimeType, fd);
    close(fd);
}

void DataSource::cancel()
{
    send_cancelled();
}

void DataSource::setManager(DataDeviceManager *mgr)
{
    m_manager = mgr;
}

void DataSource::setDevice(DataDevice *device)
{
    m_device = device;
}

DataSource *DataSource::fromResource(struct ::wl_resource *resource)
{
    return static_cast<DataSource *>(Resource::fromResource(resource)->data_source_object);
}

void DataSource::data_source_offer(Resource *, const QString &mime_type)
{
    m_mimeTypes.append(mime_type);
}

void DataSource::data_source_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void DataSource::data_source_destroy_resource(QtWaylandServer::wl_data_source::Resource *resource)
{
    Q_UNUSED(resource);
    delete this;
}

}

QT_END_NAMESPACE
