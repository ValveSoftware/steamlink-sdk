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

    ColorGradient {
        id: initial
    }

    ColorGradient {
        id: initialized
        stops: [
            ColorGradientStop { color: "blue"; position: 0 },
            ColorGradientStop { color: "white"; position: 0.5 },
            ColorGradientStop { color: "red"; position: 1 }
        ]
    }

    ColorGradient {
        id: change
    }

    TestCase {
        name: "ColorGradient Initial"

        function test_initial() {
            compare(initial.stops.length, 0)
        }
    }

    TestCase {
        name: "ColorGradient Initialized"

        function test_initialized() {
            compare(initialized.stops.length, 3)
            compare(initialized.stops[0].color, "#0000ff")
            compare(initialized.stops[1].color, "#ffffff")
            compare(initialized.stops[2].color, "#ff0000")
        }
    }

    TestCase {
        name: "ColorGradient Change"

        ColorGradientStop { id: stop1; color: "blue"; position: 0 }
        ColorGradientStop { id: stop2; color: "red"; position: 1.0 }
        ColorGradientStop { id: stop3; color: "white"; position: 0.5 }

        function test_change() {
            change.stops = [stop1]
            compare(change.stops.length, 1)
            change.stops = [stop1, stop2]
            compare(change.stops.length, 2)
            compare(change.stops[0].color, "#0000ff")
            change.stops[0].color = "red"
            compare(change.stops[0].color, "#ff0000")
            compare(change.stops[1].color, "#ff0000")
            change.stops = [stop1, stop2, stop3]
            compare(change.stops[2].color, "#ffffff")
            compare(change.stops.length, 3)
            stop2.position = 0.25
            stop3.position = 1.0
            compare(change.stops[1].position, 0.25)
            compare(change.stops[2].position, 1.0)
        }
    }
}
