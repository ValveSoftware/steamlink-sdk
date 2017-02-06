/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
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
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwaylandxdgpopup_p.h"

#include "qwaylandwindow_p.h"
#include "qwaylanddisplay_p.h"
#include "qwaylandextendedsurface_p.h"

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

QWaylandXdgPopup::QWaylandXdgPopup(struct ::xdg_popup *popup, QWaylandWindow *window)
    : QWaylandShellSurface(window)
    , QtWayland::xdg_popup(popup)
    , m_extendedWindow(nullptr)
    , m_window(window)
{
    if (window->display()->windowExtension())
        m_extendedWindow = new QWaylandExtendedSurface(window);
}

QWaylandXdgPopup::~QWaylandXdgPopup()
{
    xdg_popup_destroy(object());
    delete m_extendedWindow;
}

void QWaylandXdgPopup::setType(Qt::WindowType type, QWaylandWindow *transientParent)
{
    Q_UNUSED(type);
    Q_UNUSED(transientParent);
}

void QWaylandXdgPopup::xdg_popup_popup_done()
{
    m_window->window()->close();
}

}

QT_END_NAMESPACE
