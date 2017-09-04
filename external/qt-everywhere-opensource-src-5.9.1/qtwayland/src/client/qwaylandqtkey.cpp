/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qwaylandqtkey_p.h"
#include "qwaylandinputdevice_p.h"
#include "qwaylanddisplay_p.h"

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

QWaylandQtKeyExtension::QWaylandQtKeyExtension(QWaylandDisplay *display, uint32_t id)
    : QtWayland::qt_key_extension(display->wl_registry(), id, 2)
    , m_display(display)
{
}

void QWaylandQtKeyExtension::key_extension_qtkey(struct wl_surface *surface,
                                                 uint32_t time,
                                                 uint32_t type,
                                                 uint32_t key,
                                                 uint32_t modifiers,
                                                 uint32_t nativeScanCode,
                                                 uint32_t nativeVirtualKey,
                                                 uint32_t nativeModifiers,
                                                 const QString &text,
                                                 uint32_t autorep,
                                                 uint32_t count)
{
    QList<QWaylandInputDevice *> inputDevices = m_display->inputDevices();
    if (!surface && inputDevices.isEmpty()) {
        qWarning("qt_key_extension: handle_qtkey: No input device");
        return;
    }

    QWaylandInputDevice *dev = inputDevices.first();
    QWaylandWindow *win = surface ? QWaylandWindow::fromWlSurface(surface) : dev->keyboardFocus();

    if (!win || !win->window()) {
        qWarning("qt_key_extension: handle_qtkey: No keyboard focus");
        return;
    }

    QWindow *window = win->window();
    QWindowSystemInterface::handleExtendedKeyEvent(window, time, QEvent::Type(type), key, Qt::KeyboardModifiers(modifiers),
                                                   nativeScanCode, nativeVirtualKey, nativeModifiers, text,
                                                   autorep, count);
}

}

QT_END_NAMESPACE
