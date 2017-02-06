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

    Bar3DSeries {
        id: initial
    }

    ColorGradient {
        id: gradient1;
        stops: [
            ColorGradientStop { color: "red"; position: 0 },
            ColorGradientStop { color: "blue"; position: 1 }
        ]
    }

    ColorGradient {
        id: gradient2;
        stops: [
            ColorGradientStop { color: "green"; position: 0 },
            ColorGradientStop { color: "red"; position: 1 }
        ]
    }

    ColorGradient {
        id: gradient3;
        stops: [
            ColorGradientStop { color: "gray"; position: 0 },
            ColorGradientStop { color: "darkgray"; position: 1 }
        ]
    }

    Bar3DSeries {
        id: initialized
        dataProxy: ItemModelBarDataProxy {
            itemModel: ListModel {
                ListElement{ year: "2012"; city: "Oulu"; expenses: "4200"; }
                ListElement{ year: "2012"; city: "Rauma"; expenses: "2100"; }
            }
            rowRole: "city"
            columnRole: "year"
            valueRole: "expenses"
        }
        meshAngle: 15.0
        selectedBar: Qt.point(0, 0)

        baseColor: "blue"
        baseGradient: gradient1
        colorStyle: Theme3D.ColorStyleObjectGradient
        itemLabelFormat: "%f"
        itemLabelVisible: false
        mesh: Abstract3DSeries.MeshCone
        meshSmooth: true
        multiHighlightColor: "green"
        multiHighlightGradient: gradient2
        name: "series1"
        singleHighlightColor: "red"
        singleHighlightGradient: gradient3
        userDefinedMesh: ":/customitem.obj"
        visible: false
    }

    ItemModelBarDataProxy {
        id: proxy1
        itemModel: ListModel {
            ListElement{ year: "2012"; city: "Oulu"; expenses: "4200"; }
            ListElement{ year: "2012"; city: "Rauma"; expenses: "2100"; }
            ListElement{ year: "2012"; city: "Helsinki"; expenses: "7040"; }
        }
        rowRole: "city"
        columnRole: "year"
        valueRole: "expenses"
    }

    Bar3DSeries {
        id: change
    }

    TestCase {
        name: "Bar3DSeries Initial"

        function test_1_initial() {
            compare(initial.dataProxy.rowCount, 0)
            compare(initial.invalidSelectionPosition, Qt.point(-1, -1))
            compare(initial.meshAngle, 0)
            compare(initial.selectedBar, Qt.point(-1, -1))
        }

        function test_2_initial_common() {
            // Common properties
            compare(initial.baseColor, "#000000")
            compare(initial.baseGradient, null)
            compare(initial.colorStyle, Theme3D.ColorStyleUniform)
            compare(initial.itemLabel, "")
            compare(initial.itemLabelFormat, "@valueLabel")
            compare(initial.itemLabelVisible, true)
            compare(initial.mesh, Abstract3DSeries.MeshBevelBar)
            compare(initial.meshRotation, Qt.quaternion(1, 0, 0, 0))
            compare(initial.meshSmooth, false)
            compare(initial.multiHighlightColor, "#000000")
            compare(initial.multiHighlightGradient, null)
            compare(initial.name, "")
            compare(initial.singleHighlightColor, "#000000")
            compare(initial.singleHighlightGradient, null)
            compare(initial.type, Abstract3DSeries.SeriesTypeBar)
            compare(initial.userDefinedMesh, "")
            compare(initial.visible, true)
        }
    }

    TestCase {
        name: "Bar3DSeries Initialized"

        function test_1_initialized() {
            compare(initialized.dataProxy.rowCount, 2)
            fuzzyCompare(initialized.meshAngle, 15.0, 0.01)
            compare(initialized.selectedBar, Qt.point(0, 0))
        }

        function test_2_initialized_common() {
            // Common properties
            compare(initialized.baseColor, "#0000ff")
            compare(initialized.baseGradient, gradient1)
            compare(initialized.colorStyle, Theme3D.ColorStyleObjectGradient)
            compare(initialized.itemLabelFormat, "%f")
            compare(initialized.itemLabelVisible, false)
            compare(initialized.mesh, Abstract3DSeries.MeshCone)
            compare(initialized.meshSmooth, true)
            compare(initialized.multiHighlightColor, "#008000")
            compare(initialized.multiHighlightGradient, gradient2)
            compare(initialized.name, "series1")
            compare(initialized.singleHighlightColor, "#ff0000")
            compare(initialized.singleHighlightGradient, gradient3)
            compare(initialized.userDefinedMesh, ":/customitem.obj")
            compare(initialized.visible, false)
        }
    }

    TestCase {
        name: "Bar3DSeries Change"

        function test_1_change() {
            change.dataProxy = proxy1
            change.meshAngle = 15.0
            change.selectedBar = Qt.point(0, 0)
        }

        function test_2_test_change() {
            // This test has a dependency to the previous one due to asynchronous item model resolving
            compare(change.dataProxy.rowCount, 3)
            fuzzyCompare(change.meshAngle, 15.0, 0.01)
            compare(change.selectedBar, Qt.point(0, 0))
        }

        function test_3_change_common() {
            change.baseColor = "blue"
            change.baseGradient = gradient1
            change.colorStyle = Theme3D.ColorStyleObjectGradient
            change.itemLabelFormat = "%f"
            change.itemLabelVisible = false
            change.mesh = Abstract3DSeries.MeshCone
            change.meshSmooth = true
            change.multiHighlightColor = "green"
            change.multiHighlightGradient = gradient2
            change.name = "series1"
            change.singleHighlightColor = "red"
            change.singleHighlightGradient = gradient3
            change.userDefinedMesh = ":/customitem.obj"
            change.visible = false

            compare(change.baseColor, "#0000ff")
            compare(change.baseGradient, gradient1)
            compare(change.colorStyle, Theme3D.ColorStyleObjectGradient)
            compare(change.itemLabelFormat, "%f")
            compare(change.itemLabelVisible, false)
            compare(change.mesh, Abstract3DSeries.MeshCone)
            compare(change.meshSmooth, true)
            compare(change.multiHighlightColor, "#008000")
            compare(change.multiHighlightGradient, gradient2)
            compare(change.name, "series1")
            compare(change.singleHighlightColor, "#ff0000")
            compare(change.singleHighlightGradient, gradient3)
            compare(change.userDefinedMesh, ":/customitem.obj")
            compare(change.visible, false)
        }

        function test_4_change_gradient_stop() {
            gradient1.stops[0].color = "yellow"
            compare(change.baseGradient.stops[0].color, "#ffff00")
        }
    }
}
