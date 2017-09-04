/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
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

#include "qwldataoffer_p.h"

#include "qwldatadevice_p.h"
#include "qwldatasource_p.h"

#include <unistd.h>

QT_BEGIN_NAMESPACE

namespace QtWayland
{

DataOffer::DataOffer(DataSource *dataSource, QtWaylandServer::wl_data_device::Resource *target)
    : QtWaylandServer::wl_data_offer(target->client(), 0, 1)
    , m_dataSource(dataSource)
{
    // FIXME: connect to dataSource and reset m_dataSource on destroy
    target->data_device_object->send_data_offer(target->handle, resource()->handle);
    Q_FOREACH (const QString &mimeType, dataSource->mimeTypes()) {
        send_offer(mimeType);
    }
}

DataOffer::~DataOffer()
{
}

void DataOffer::data_offer_accept(Resource *resource, uint32_t serial, const QString &mimeType)
{
    Q_UNUSED(resource);
    Q_UNUSED(serial);
    if (m_dataSource)
        m_dataSource->accept(mimeType);
}

void DataOffer::data_offer_receive(Resource *resource, const QString &mimeType, int32_t fd)
{
    Q_UNUSED(resource);
    if (m_dataSource)
        m_dataSource->send(mimeType, fd);
    else
        close(fd);
}

void DataOffer::data_offer_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void DataOffer::data_offer_destroy_resource(Resource *)
{
    delete this;
}

}

QT_END_NAMESPACE
