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

namespace Impl {

void Compositor::bindOutput(wl_client *client, void *compositorData, uint32_t version, uint32_t id)
{
    Q_UNUSED(version);

    wl_resource *resource = wl_client_add_object(client, &wl_output_interface, 0, id, compositorData);

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

