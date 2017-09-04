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

    property var bars3d: null

    function constructBars() {
        bars3d = Qt.createQmlObject("
        import QtQuick 2.2
        import QtDataVisualization 1.2
        Bars3D {
            anchors.fill: parent
        }", top)
        bars3d.anchors.fill = top
    }

    TestCase {
        name: "Bars3D Series"
        when: windowShown

        Bar3DSeries { id: series1 }
        Bar3DSeries { id: series2 }

        function test_1_add_series() {
            constructBars()

            bars3d.seriesList = [series1, series2]
            compare(bars3d.seriesList.length, 2)
            console.log("top:", top)
            waitForRendering(top)
        }

        function test_2_remove_series() {
            bars3d.seriesList = [series1]
            compare(bars3d.seriesList.length, 1)
            waitForRendering(top)
        }

        function test_3_remove_series() {
            bars3d.seriesList = []
            compare(bars3d.seriesList.length, 0)
            waitForRendering(top)
        }

        function test_4_primary_series() {
            bars3d.seriesList = [series1, series2]
            compare(bars3d.primarySeries, series1)
            bars3d.primarySeries = series2
            compare(bars3d.primarySeries, series2)
            waitForRendering(top)
        }

        function test_5_selected_series() {
            bars3d.seriesList[0].selectedBar = Qt.point(0, 0)
            compare(bars3d.selectedSeries, series1)

            waitForRendering(top)
            bars3d.destroy()
            waitForRendering(top)
        }
    }

    // The following tests are not required for scatter or surface, as they are handled identically

    Custom3DItem { id: item1; meshFile: ":/customitem.obj" }
    Custom3DItem { id: item2; meshFile: ":/customitem.obj" }
    Custom3DItem { id: item3; meshFile: ":/customitem.obj" }
    Custom3DItem { id: item4; meshFile: ":/customitem.obj"; position: Qt.vector3d(0.0, 1.0, 0.0) }

    function constructBarsWithCustomItemList() {
        bars3d = Qt.createQmlObject("
        import QtQuick 2.2
        import QtDataVisualization 1.2
        Bars3D {
            anchors.fill: parent
            customItemList: [item1, item2]
        }", top)
        bars3d.anchors.fill = top
    }

    TestCase {
        name: "Bars3D Theme"
        when: windowShown

        Theme3D { id: newTheme }

        function test_1_add_theme() {
            constructBars()

            bars3d.theme = newTheme
            compare(bars3d.theme, newTheme)
            waitForRendering(top)
        }

        function test_2_change_theme() {
            newTheme.type = Theme3D.ThemePrimaryColors
            compare(bars3d.theme.type, Theme3D.ThemePrimaryColors)

            waitForRendering(top)
            bars3d.destroy()
            waitForRendering(top)
        }
    }

    TestCase {
        name: "Bars3D Input"
        when: windowShown

        function test_1_remove_input() {
            constructBars()
            bars3d.inputHandler = null
            compare(bars3d.inputHandler, null)

            waitForRendering(top)
            bars3d.destroy()
            waitForRendering(top)
        }
    }

    TestCase {
        name: "Bars3D Custom"
        when: windowShown

        function test_1_custom_items() {
            constructBarsWithCustomItemList()
            compare(bars3d.customItemList.length, 2)
            waitForRendering(top)
        }

        function test_2_add_custom_items() {
            bars3d.addCustomItem(item3)
            compare(bars3d.customItemList.length, 3)
            bars3d.addCustomItem(item4)
            compare(bars3d.customItemList.length, 4)
            waitForRendering(top)
        }

        function test_3_change_custom_items() {
            item1.position = Qt.vector3d(1.0, 1.0, 1.0)
            compare(bars3d.customItemList[0].position, Qt.vector3d(1.0, 1.0, 1.0))
            waitForRendering(top)
        }

        function test_4_remove_custom_items() {
            bars3d.removeCustomItemAt(Qt.vector3d(0.0, 1.0, 0.0))
            compare(bars3d.customItemList.length, 3)
            bars3d.releaseCustomItem(item1)
            compare(bars3d.customItemList[0], item2)
            bars3d.releaseCustomItem(item2)
            compare(bars3d.customItemList.length, 1)
            bars3d.removeCustomItems()
            compare(bars3d.customItemList.length, 0)

            waitForRendering(top)
            bars3d.destroy()
            waitForRendering(top)
        }
    }
}
