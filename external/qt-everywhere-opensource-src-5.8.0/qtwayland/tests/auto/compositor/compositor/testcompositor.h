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

#include "qwaylandcompositor.h"
#include "qwaylandsurface.h"
#include "qwaylandwlshell.h"

class TestCompositor : public QWaylandCompositor
{
    Q_OBJECT
public:
    TestCompositor(bool createInputDev = false);
    void create();
    void flushClients();

public slots:
    void onSurfaceCreated(QWaylandSurface *surface);
    void onSurfaceAboutToBeDestroyed(QWaylandSurface *surface);

protected:
    QWaylandSeat *createSeat() Q_DECL_OVERRIDE;
    QWaylandKeyboard *createKeyboardDevice(QWaylandSeat *seat) Q_DECL_OVERRIDE;

public:
    QList<QWaylandSurface *> surfaces;
    QWaylandWlShell* shell;
    bool m_createSeat;
};

