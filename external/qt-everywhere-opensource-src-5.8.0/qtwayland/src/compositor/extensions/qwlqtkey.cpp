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

#include "qwlqtkey_p.h"
#include <QtWaylandCompositor/QWaylandSurface>
#include <QKeyEvent>
#include <QWindow>

QT_BEGIN_NAMESPACE

namespace QtWayland {

QtKeyExtensionGlobal::QtKeyExtensionGlobal(QWaylandCompositor *compositor)
    : QWaylandCompositorExtensionTemplate(compositor)
    , QtWaylandServer::qt_key_extension(compositor->display(), 2)
    , m_compositor(compositor)
{
}

bool QtKeyExtensionGlobal::postQtKeyEvent(QKeyEvent *event, QWaylandSurface *surface)
{
    uint32_t time = m_compositor->currentTimeMsecs();

    Resource *target = surface ? resourceMap().value(surface->waylandClient()) : 0;

    if (target) {
        send_qtkey(target->handle,
                   surface ? surface->resource() : 0,
                   time, event->type(), event->key(), event->modifiers(),
                   event->nativeScanCode(),
                   event->nativeVirtualKey(),
                   event->nativeModifiers(),
                   event->text(),
                   event->isAutoRepeat(),
                   event->count());

        return true;
    }

    return false;
}

}

QT_END_NAMESPACE
