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

    ValueAxis3D {
        id: initial
    }

    ValueAxis3D {
        id: initialized
        formatter: ValueAxis3DFormatter { objectName: "formatter1" }
        labelFormat: "%f"
        reversed: true
        segmentCount: 10
        subSegmentCount: 5

        autoAdjustRange: false
        labelAutoRotation: 10.0
        max: 20
        min: -10
        title: "initialized"
        titleFixed: false
        titleVisible: true
    }

    ValueAxis3D {
        id: change
    }

    ValueAxis3D {
        id: invalid
    }

    TestCase {
        name: "ValueAxis3D Initial"

        function test_initial() {
            verify(initial.formatter)
            compare(initial.labelFormat, "%.2f")
            compare(initial.reversed, false)
            compare(initial.segmentCount, 5)
            compare(initial.subSegmentCount, 1)

            compare(initial.autoAdjustRange, true)
            compare(initial.labelAutoRotation, 0.0)
            compare(initial.max, 10)
            compare(initial.min, 0)
            compare(initial.orientation, AbstractAxis3D.AxisOrientationNone)
            compare(initial.title, "")
            compare(initial.titleFixed, true)
            compare(initial.titleVisible, false)
            compare(initial.type, AbstractAxis3D.AxisTypeValue)
        }
    }

    TestCase {
        name: "ValueAxis3D Initialized"

        function test_initialized() {
            compare(initialized.formatter.objectName, "formatter1")
            compare(initialized.labelFormat, "%f")
            compare(initialized.reversed, true)
            compare(initialized.segmentCount, 10)
            compare(initialized.subSegmentCount, 5)

            compare(initialized.autoAdjustRange, false)
            compare(initialized.labelAutoRotation, 10.0)
            compare(initialized.max, 20)
            compare(initialized.min, -10)
            compare(initialized.title, "initialized")
            compare(initialized.titleFixed, false)
            compare(initialized.titleVisible, true)
        }
    }

    TestCase {
        name: "ValueAxis3D Change"

        ValueAxis3DFormatter { id: formatter1 }

        function test_change() {
            change.formatter = formatter1
            change.labelFormat = "%f"
            change.reversed = true
            change.segmentCount = 10
            change.subSegmentCount = 5

            compare(change.formatter, formatter1)
            compare(change.labelFormat, "%f")
            compare(change.reversed, true)
            compare(change.segmentCount, 10)
            compare(change.subSegmentCount, 5)

            change.autoAdjustRange = false
            change.labelAutoRotation = 10.0
            change.max = 20
            change.min = -10
            change.title = "initialized"
            change.titleFixed = false
            change.titleVisible = true

            compare(change.autoAdjustRange, false)
            compare(change.labelAutoRotation, 10.0)
            compare(change.max, 20)
            compare(change.min, -10)
            compare(change.title, "initialized")
            compare(change.titleFixed, false)
            compare(change.titleVisible, true)
        }
    }

    TestCase {
        name: "ValueAxis3D Invalid"

        function test_invalid() {
            invalid.segmentCount = -1
            compare(invalid.segmentCount, 1)
            invalid.subSegmentCount = -1
            compare(invalid.subSegmentCount, 1)

            invalid.labelAutoRotation = -10
            compare(invalid.labelAutoRotation, 0.0)
            invalid.labelAutoRotation = 100
            compare(invalid.labelAutoRotation, 90.0)
            invalid.max = -10
            compare(invalid.min, -11)
            invalid.min = 10
            compare(invalid.max, 11)
        }
    }
}
