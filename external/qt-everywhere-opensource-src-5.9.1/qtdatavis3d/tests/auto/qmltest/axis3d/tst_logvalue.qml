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
    height: 150
    width: 150

    LogValueAxis3DFormatter {
        id: initial
    }

    LogValueAxis3DFormatter {
        id: initialized
        autoSubGrid: false
        base: 0.1
        showEdgeLabels: false
    }

    LogValueAxis3DFormatter {
        id: change
    }

    LogValueAxis3DFormatter {
        id: invalid
    }

    TestCase {
        name: "LogValueAxis3DFormatter Initial"

        function test_initial() {
            compare(initial.autoSubGrid, true)
            compare(initial.base, 10)
            compare(initial.showEdgeLabels, true)
        }
    }

    TestCase {
        name: "LogValueAxis3DFormatter Initialized"

        function test_initialized() {
            compare(initialized.autoSubGrid, false)
            compare(initialized.base, 0.1)
            compare(initialized.showEdgeLabels, false)
        }
    }

    TestCase {
        name: "LogValueAxis3DFormatter Change"

        function test_change() {
            change.autoSubGrid = false
            change.base = 0.1
            change.showEdgeLabels = false

            compare(change.autoSubGrid, false)
            compare(change.base, 0.1)
            compare(change.showEdgeLabels, false)
        }
    }

    TestCase {
        name: "LogValueAxis3DFormatter Invalid"

        function test_invalid() {
            invalid.base = 1
            compare(invalid.base, 10)
            invalid.base = -1
            compare(invalid.base, 10)
        }
    }
}
