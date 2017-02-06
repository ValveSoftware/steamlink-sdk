/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "mockcompositor.h"

namespace Impl {

void Compositor::bindOutput(wl_client *client, void *compositorData, uint32_t version, uint32_t id)
{
    wl_resource *resource = wl_resource_create(client, &wl_output_interface, static_cast<int>(version), id);

    Compositor *compositor = static_cast<Compositor *>(compositorData);
    registerResource(&compositor->m_outputResources, resource);

    compositor->sendOutputGeometry(resource);
    compositor->sendOutputMode(resource);
}

void Compositor::sendOutputGeometry(wl_resource *resource)
{
    const QRect &r = m_outputGeometry;
    wl_output_send_geometry(resource, r.x(), r.y(), r.width(), r.height(), 0, "", "",0);
}

void Compositor::sendOutputMode(wl_resource *resource)
{
    const QRect &r = m_outputGeometry;
    wl_output_send_mode(resource, WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED, r.width(), r.height(), 60);
}

void Compositor::setOutputGeometry(void *c, const QList<QVariant> &parameters)
{
    Compositor *compositor = static_cast<Compositor *>(c);
    compositor->m_outputGeometry = parameters.first().toRect();

    wl_resource *resource;
    wl_list_for_each(resource, &compositor->m_outputResources, link)
        compositor->sendOutputGeometry(resource);
}

}

