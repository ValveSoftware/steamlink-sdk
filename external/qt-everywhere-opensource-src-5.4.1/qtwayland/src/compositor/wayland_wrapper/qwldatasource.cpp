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

#include "qwldatasource_p.h"
#include "qwldataoffer_p.h"
#include "qwldatadevice_p.h"
#include "qwldatadevicemanager_p.h"
#include "qwlcompositor_p.h"

#include <unistd.h>
#include <QtCompositor/private/wayland-wayland-server-protocol.h>

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
