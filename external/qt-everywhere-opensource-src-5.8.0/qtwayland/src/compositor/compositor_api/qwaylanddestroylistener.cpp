/****************************************************************************
**
** Copyright (C) 2014 Jolla Ltd, author: <giulio.camuffo@jollamobile.com>
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

#include "qwaylanddestroylistener.h"
#include "qwaylanddestroylistener_p.h"

QT_BEGIN_NAMESPACE

QWaylandDestroyListenerPrivate::QWaylandDestroyListenerPrivate()
{
    listener.parent = this;
    listener.listener.notify = handler;
    wl_list_init(&listener.listener.link);
}

QWaylandDestroyListener::QWaylandDestroyListener(QObject *parent)
    : QObject(* new QWaylandDestroyListenerPrivate(), parent)
{
}
void QWaylandDestroyListener::listenForDestruction(::wl_resource *resource)
{
    Q_D(QWaylandDestroyListener);
    wl_resource_add_destroy_listener(resource, &d->listener.listener);
}

void QWaylandDestroyListener::reset()
{
    Q_D(QWaylandDestroyListener);
    wl_list_remove(&d->listener.listener.link);
    wl_list_init(&d->listener.listener.link);
}

void QWaylandDestroyListenerPrivate::handler(wl_listener *listener, void *data)
{
    QWaylandDestroyListenerPrivate *that = reinterpret_cast<Listener *>(listener)->parent;
    emit that->q_func()->fired(data);
}

QT_END_NAMESPACE
