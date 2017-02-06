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

#include "xcompositehandler.h"

#include "wayland-xcomposite-server-protocol.h"

#include "xcompositebuffer.h"
#include <X11/extensions/Xcomposite.h>

QT_BEGIN_NAMESPACE

XCompositeHandler::XCompositeHandler(QWaylandCompositor *compositor, Display *display)
    : QtWaylandServer::qt_xcomposite(compositor->display(), 1)
{
    mFakeRootWindow = new QWindow();
    mFakeRootWindow->setGeometry(QRect(-1,-1,1,1));
    mFakeRootWindow->create();
    mFakeRootWindow->show();

    int composite_event_base, composite_error_base;
    if (!XCompositeQueryExtension(display, &composite_event_base, &composite_error_base))
        qFatal("XComposite required");

    mDisplayString = QString::fromLocal8Bit(XDisplayString(display));
}

void XCompositeHandler::xcomposite_bind_resource(Resource *resource)
{
    send_root(resource->handle, mDisplayString, mFakeRootWindow->winId());
}

void XCompositeHandler::xcomposite_create_buffer(Resource *resource, uint32_t id, uint32_t window,
                                                 int32_t width, int32_t height)
{
    new XCompositeBuffer(Window(window), QSize(width, height), resource->client(), id);
}

QT_END_NAMESPACE
