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

#include "brcmbuffer.h"

#include <EGL/eglext.h>

#include <EGL/eglext_brcm.h>

QT_BEGIN_NAMESPACE

BrcmBuffer::BrcmBuffer(struct ::wl_client *client, uint32_t id, const QSize &size, EGLint *data, size_t count)
    : QtWaylandServer::wl_buffer(client, id, 1)
    , m_handle(count)
    , m_invertedY(false)
    , m_size(size)
{
    for (size_t i = 0; i < count; ++i)
        m_handle[i] = data[i];
}

BrcmBuffer::~BrcmBuffer()
{
    static PFNEGLDESTROYGLOBALIMAGEBRCMPROC eglDestroyGlobalImage =
        (PFNEGLDESTROYGLOBALIMAGEBRCMPROC) eglGetProcAddress("eglDestroyGlobalImageBRCM");
    eglDestroyGlobalImage(handle());
}

void BrcmBuffer::buffer_destroy_resource(Resource *)
{
    delete this;
}

void BrcmBuffer::buffer_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

QT_END_NAMESPACE
