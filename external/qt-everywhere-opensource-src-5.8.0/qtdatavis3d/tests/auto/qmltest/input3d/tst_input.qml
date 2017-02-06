/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtDataVisualization 1.2
import QtTest 1.0

Item {
    id: top
    width: 150
    height: 150

    InputHandler3D {
        id: initial
    }

    InputHandler3D {
        id: initialized
        rotationEnabled: false
        selectionEnabled: false
        zoomAtTargetEnabled: false
        zoomEnabled: false
    }

    InputHandler3D {
        id: change
    }

    TestCase {
        name: "InputHandler3D Initial"

        function test_initial() {
            compare(initial.rotationEnabled, true)
            compare(initial.selectionEnabled, true)
            compare(initial.zoomAtTargetEnabled, true)
            compare(initial.zoomEnabled, true)
        }
    }

    TestCase {
        name: "InputHandler3D Initialized"

        function test_initialized() {
            compare(initialized.rotationEnabled, false)
            compare(initialized.selectionEnabled, false)
            compare(initialized.zoomAtTargetEnabled, false)
            compare(initialized.zoomEnabled, false)
        }
    }

    TestCase {
        name: "InputHandler3D Change"

        function test_change() {
            change.rotationEnabled = false
            change.selectionEnabled = false
            change.zoomAtTargetEnabled = false
            change.zoomEnabled = false

            compare(change.rotationEnabled, false)
            compare(change.selectionEnabled, false)
            compare(change.zoomAtTargetEnabled, false)
            compare(change.zoomEnabled, false)
        }

        // TODO: QTRD-3380 (mouse events)
    }
}
