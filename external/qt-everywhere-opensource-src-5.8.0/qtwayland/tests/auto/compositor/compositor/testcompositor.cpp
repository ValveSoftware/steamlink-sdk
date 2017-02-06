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

#include "testcompositor.h"
#include "testseat.h"
#include "testkeyboardgrabber.h"

#include <wayland-server.h>

TestCompositor::TestCompositor(bool createInputDev)
    : QWaylandCompositor()
    , shell(new QWaylandWlShell(this))
    , m_createSeat(createInputDev)
{
    setSocketName("wayland-qt-test-0");
}

void TestCompositor::create()
{
    new QWaylandOutput(this, Q_NULLPTR);
    QWaylandCompositor::create();

    connect(this, &QWaylandCompositor::surfaceCreated, this, &TestCompositor::onSurfaceCreated);
    connect(this, &QWaylandCompositor::surfaceAboutToBeDestroyed, this, &TestCompositor::onSurfaceAboutToBeDestroyed);
}

void TestCompositor::flushClients()
{
    wl_display_flush_clients(display());
}

void TestCompositor::onSurfaceCreated(QWaylandSurface *surface)
{
    surfaces << surface;
}

void TestCompositor::onSurfaceAboutToBeDestroyed(QWaylandSurface *surface)
{
    surfaces.removeOne(surface);
}

QWaylandSeat *TestCompositor::createSeat()
{
    if (m_createSeat)
        return new TestSeat(this, QWaylandSeat::Pointer | QWaylandSeat::Keyboard);
    else
        return QWaylandCompositor::createSeat();
}

QWaylandKeyboard *TestCompositor::createKeyboardDevice(QWaylandSeat *seat) {
    return new TestKeyboardGrabber(seat);
}
