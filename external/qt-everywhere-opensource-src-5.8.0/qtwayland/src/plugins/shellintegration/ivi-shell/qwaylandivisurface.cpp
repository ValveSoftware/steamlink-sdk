/****************************************************************************
**
** Copyright (C) 2015 ITAGE Corporation, author: <yusuke.binsaki@itage.co.jp>
** Contact: http://www.qt-project.org/legal
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

#include "qwaylandivisurface_p.h"

#include <QtWaylandClient/private/qwaylanddisplay_p.h>
#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <QtWaylandClient/private/qwaylandscreen_p.h>
#include <QtWaylandClient/private/qwaylandextendedsurface_p.h>

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

QWaylandIviSurface::QWaylandIviSurface(struct ::ivi_surface *ivi_surface, QWaylandWindow *window)
    : QtWayland::ivi_surface(ivi_surface)
    , QWaylandShellSurface(window)
    , m_window(window)
    , m_extendedWindow(Q_NULLPTR)
{
    createExtendedSurface(window);
}

QWaylandIviSurface::QWaylandIviSurface(struct ::ivi_surface *ivi_surface, QWaylandWindow *window,
                                       struct ::ivi_controller_surface *iviControllerSurface)
    : QtWayland::ivi_surface(ivi_surface)
    , QWaylandShellSurface(window)
    , QtWayland::ivi_controller_surface(iviControllerSurface)
    , m_window(window)
    , m_extendedWindow(Q_NULLPTR)
{
    createExtendedSurface(window);
}

QWaylandIviSurface::~QWaylandIviSurface()
{
    ivi_surface::destroy();
    if (QtWayland::ivi_controller_surface::object())
        QtWayland::ivi_controller_surface::destroy(0);

    delete m_extendedWindow;
}

void QWaylandIviSurface::setType(Qt::WindowType type, QWaylandWindow *transientParent)
{

    Q_UNUSED(type)
    Q_UNUSED(transientParent)
}

void QWaylandIviSurface::createExtendedSurface(QWaylandWindow *window)
{
    if (window->display()->windowExtension())
        m_extendedWindow = new QWaylandExtendedSurface(window);
}

void QWaylandIviSurface::ivi_surface_configure(int32_t width, int32_t height)
{
    this->m_window->configure(0, width, height);
}

}

QT_END_NAMESPACE
