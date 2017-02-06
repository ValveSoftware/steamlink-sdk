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

#ifndef XCOMPOSITEBUFFER_H
#define XCOMPOSITEBUFFER_H

#include <qwayland-server-wayland.h>
#include <QtWaylandCompositor/QWaylandSurface>
#include <QtWaylandCompositor/QWaylandCompositor>

#include <QtCore/QSize>

#include <QtCore/QTextStream>
#include <QtCore/QDataStream>
#include <QtCore/QMetaType>
#include <QtCore/QVariant>

#include <X11/X.h>

QT_BEGIN_NAMESPACE

class XCompositeBuffer : public QtWaylandServer::wl_buffer
{
public:
    XCompositeBuffer(Window window, const QSize &size,
                     struct ::wl_client *client, uint32_t id);

    Window window();

    QWaylandSurface::Origin origin() const { return mOrigin; }
    void setOrigin(QWaylandSurface::Origin origin) { mOrigin = origin; }

    QSize size() const { return mSize; }

    static XCompositeBuffer *fromResource(struct ::wl_resource *resource) { return static_cast<XCompositeBuffer*>(Resource::fromResource(resource)->buffer_object); }

protected:
    void buffer_destroy_resource(Resource *) Q_DECL_OVERRIDE;
    void buffer_destroy(Resource *) Q_DECL_OVERRIDE;

private:
    Window mWindow;
    QWaylandSurface::Origin mOrigin;
    QSize mSize;
};

QT_END_NAMESPACE

#endif // XCOMPOSITORBUFFER_H
