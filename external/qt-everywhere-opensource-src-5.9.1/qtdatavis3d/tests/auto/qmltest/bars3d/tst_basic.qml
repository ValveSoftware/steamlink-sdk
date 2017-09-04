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

    property var empty: null
    property var basic: null
    property var common: null
    property var common_init: null

    function constructEmpty() {
        empty = Qt.createQmlObject("
        import QtQuick 2.2
        import QtDataVisualization 1.2
        Bars3D {
        }", top)
    }

    function constructBasic() {
        basic = Qt.createQmlObject("
        import QtQuick 2.2
        import QtDataVisualization 1.2
        Bars3D {
            anchors.fill: parent
            multiSeriesUniform: true
            barThickness: 0.1
            barSpacing.width: 0.1
            barSpacing.height: 0.1
            barSpacingRelative: false
            floorLevel: 1.0
        }", top)
        basic.anchors.fill = top
    }

    function constructCommon() {
        common = Qt.createQmlObject("
        import QtQuick 2.2
        import QtDataVisualization 1.2
        Bars3D {
            anchors.fill: parent
        }", top)
        common.anchors.fill = top
    }

    function constructCommonInit() {
        common_init = Qt.createQmlObject("
        import QtQuick 2.2
        import QtDataVisualization 1.2
        Bars3D {
            anchors.fill: parent
            selectionMode: AbstractGraph3D.SelectionNone
            shadowQuality: AbstractGraph3D.ShadowQualityLow
            msaaSamples: 2
            theme: Theme3D { }
            renderingMode: AbstractGraph3D.RenderIndirect
            measureFps: true
            orthoProjection: false
            aspectRatio: 3.0
            optimizationHints: AbstractGraph3D.OptimizationStatic
            polar: false
            radialLabelOffset: 2
            horizontalAspectRatio: 0.2
            reflection: true
            reflectivity: 0.1
            locale: Qt.locale(\"UK\")
            margin: 0.2
        }", top)
        common_init.anchors.fill = top
    }

    TestCase {
        name: "Bars3D Empty"
        when: windowShown

        function test_empty() {
            constructEmpty()
            compare(empty.width, 0, "width")
            compare(empty.height, 0, "height")
            compare(empty.multiSeriesUniform, false, "multiSeriesUniform")
            compare(empty.barThickness, 1.0, "barThickness")
            compare(empty.barSpacing, Qt.size(1.0, 1.0), "barSpacing")
            compare(empty.barSpacingRelative, true, "barSpacingRelative")
            compare(empty.seriesList.length, 0, "seriesList")
            compare(empty.selectedSeries, null, "selectedSeries")
            compare(empty.primarySeries, null, "primarySeries")
            compare(empty.floorLevel, 0.0, "floorLevel")
            compare(empty.columnAxis.orientation, AbstractAxis3D.AxisOrientationX)
            compare(empty.rowAxis.orientation, AbstractAxis3D.AxisOrientationZ)
            compare(empty.valueAxis.orientation, AbstractAxis3D.AxisOrientationY)
            compare(empty.columnAxis.type, AbstractAxis3D.AxisTypeCategory)
            compare(empty.rowAxis.type, AbstractAxis3D.AxisTypeCategory)
            compare(empty.valueAxis.type, AbstractAxis3D.AxisTypeValue)
            waitForRendering(top)
            empty.destroy()
            waitForRendering(top)
        }
    }

    TestCase {
        name: "Bars3D Basic"
        when: windowShown

        function test_1_basic() {
            constructBasic()
            compare(basic.width, 150, "width")
            compare(basic.height, 150, "height")
            compare(basic.multiSeriesUniform, true, "multiSeriesUniform")
            compare(basic.barThickness, 0.1, "barThickness")
            compare(basic.barSpacing, Qt.size(0.1, 0.1), "barSpacing")
            compare(basic.barSpacingRelative, false, "barSpacingRelative")
            compare(basic.floorLevel, 1.0, "floorLevel")
            waitForRendering(top)
        }

        function test_2_basic_change() {
            basic.multiSeriesUniform = false
            basic.barThickness = 0.5
            basic.barSpacing = Qt.size(1.0, 0.0)
            basic.barSpacingRelative = true
            basic.floorLevel = 0.2
            compare(basic.multiSeriesUniform, false, "multiSeriesUniform")
            compare(basic.barThickness, 0.5, "barThickness")
            compare(basic.barSpacing, Qt.size(1.0, 0.0), "barSpacing")
            compare(basic.barSpacingRelative, true, "barSpacingRelative")
            compare(basic.floorLevel, 0.2, "floorLevel")
            waitForRendering(top)
        }

        function test_3_basic_change_invalid() {
            basic.barThickness = -1
            basic.barSpacing = Qt.size(-1.0, -1.0)
            compare(basic.barThickness, -1/*0.5*/, "barThickness") // TODO: Fix once QTRD-3367 is done
            compare(basic.barSpacing, Qt.size(-1.0, -1.0), "barSpacing")
            waitForRendering(top)
            basic.destroy()
            waitForRendering(top)
        }
    }

    TestCase {
        name: "Bars3D Common"
        when: windowShown

        function test_1_common() {
            constructCommon()
            compare(common.selectionMode, AbstractGraph3D.SelectionItem, "selectionMode")
            compare(common.shadowQuality, AbstractGraph3D.ShadowQualityMedium, "shadowQuality")
            if (common.shadowsSupported === true)
                compare(common.msaaSamples, 4, "msaaSamples")
            else
                compare(common.msaaSamples, 0, "msaaSamples")
            compare(common.theme.type, Theme3D.ThemeQt, "theme")
            compare(common.renderingMode, AbstractGraph3D.RenderIndirect, "renderingMode")
            compare(common.measureFps, false, "measureFps")
            compare(common.customItemList.length, 0, "customItemList")
            compare(common.orthoProjection, false, "orthoProjection")
            compare(common.selectedElement, AbstractGraph3D.ElementNone, "selectedElement")
            compare(common.aspectRatio, 2.0, "aspectRatio")
            compare(common.optimizationHints, AbstractGraph3D.OptimizationDefault, "optimizationHints")
            compare(common.polar, false, "polar")
            compare(common.radialLabelOffset, 1, "radialLabelOffset")
            compare(common.horizontalAspectRatio, 0, "horizontalAspectRatio")
            compare(common.reflection, false, "reflection")
            compare(common.reflectivity, 0.5, "reflectivity")
            compare(common.locale, Qt.locale("C"), "locale")
            compare(common.queriedGraphPosition, Qt.vector3d(0, 0, 0), "queriedGraphPosition")
            compare(common.margin, -1, "margin")
            waitForRendering(top)
        }

        function test_2_change_common() {
            common.selectionMode = AbstractGraph3D.SelectionItem | AbstractGraph3D.SelectionRow | AbstractGraph3D.SelectionSlice
            common.shadowQuality = AbstractGraph3D.ShadowQualitySoftHigh
            compare(common.shadowQuality, AbstractGraph3D.ShadowQualitySoftHigh, "shadowQuality")
            common.msaaSamples = 8
            if (common.shadowsSupported === true)
                compare(common.msaaSamples, 8, "msaaSamples")
            else
                compare(common.msaaSamples, 0, "msaaSamples")
            common.theme.type = Theme3D.ThemeRetro
            common.renderingMode = AbstractGraph3D.RenderDirectToBackground_NoClear
            common.measureFps = true
            common.orthoProjection = true
            common.aspectRatio = 1.0
            common.optimizationHints = AbstractGraph3D.OptimizationStatic
            common.polar = true
            common.radialLabelOffset = 2
            common.horizontalAspectRatio = 1
            common.reflection = true
            common.reflectivity = 1.0
            common.locale = Qt.locale("FI")
            common.margin = 1.0
            compare(common.selectionMode, AbstractGraph3D.SelectionItem | AbstractGraph3D.SelectionRow | AbstractGraph3D.SelectionSlice, "selectionMode")
            compare(common.shadowQuality, AbstractGraph3D.ShadowQualityNone, "shadowQuality") // Ortho disables shadows
            compare(common.msaaSamples, 0, "msaaSamples") // Rendering mode changes this to zero
            compare(common.theme.type, Theme3D.ThemeRetro, "theme")
            compare(common.renderingMode, AbstractGraph3D.RenderDirectToBackground_NoClear, "renderingMode")
            compare(common.measureFps, true, "measureFps")
            compare(common.orthoProjection, true, "orthoProjection")
            compare(common.aspectRatio, 1.0, "aspectRatio")
            compare(common.optimizationHints, AbstractGraph3D.OptimizationStatic, "optimizationHints")
            compare(common.polar, true, "polar")
            compare(common.radialLabelOffset, 2, "radialLabelOffset")
            compare(common.horizontalAspectRatio, 1, "horizontalAspectRatio")
            compare(common.reflection, true, "reflection")
            compare(common.reflectivity, 1.0, "reflectivity")
            compare(common.locale, Qt.locale("FI"), "locale")
            compare(common.margin, 1.0, "margin")
            waitForRendering(top)
        }

        function test_3_change_invalid_common() {
            common.selectionMode = AbstractGraph3D.SelectionRow | AbstractGraph3D.SelectionColumn | AbstractGraph3D.SelectionSlice
            common.theme.type = -2
            common.renderingMode = -1
            common.measureFps = false
            common.orthoProjection = false
            common.aspectRatio = -1.0
            common.polar = false
            common.horizontalAspectRatio = -2
            common.reflection = false
            common.reflectivity = -1.0
            compare(common.selectionMode, AbstractGraph3D.SelectionItem | AbstractGraph3D.SelectionRow | AbstractGraph3D.SelectionSlice, "selectionMode")
            compare(common.theme.type, -2/*Theme3D.ThemeRetro*/, "theme") // TODO: Fix once QTRD-3367 is done
            compare(common.renderingMode, -1/*AbstractGraph3D.RenderDirectToBackground_NoClear*/, "renderingMode") // TODO: Fix once QTRD-3367 is done
            compare(common.aspectRatio, -1.0/*1.0*/, "aspectRatio") // TODO: Fix once QTRD-3367 is done
            compare(common.horizontalAspectRatio, -2/*1*/, "horizontalAspectRatio") // TODO: Fix once QTRD-3367 is done
            compare(common.reflectivity, -1.0/*1.0*/, "reflectivity") // TODO: Fix once QTRD-3367 is done

            waitForRendering(top)
            common.destroy()
            waitForRendering(top)
        }

        function test_4_common_initialized() {
            constructCommonInit()

            compare(common_init.selectionMode, AbstractGraph3D.SelectionNone, "selectionMode")
            if (common_init.shadowsSupported === true) {
                tryCompare(common_init, "shadowQuality", AbstractGraph3D.ShadowQualityLow)
                compare(common_init.msaaSamples, 2, "msaaSamples")
            } else {
                tryCompare(common_init, "shadowQuality", AbstractGraph3D.ShadowQualityNone)
                compare(common_init.msaaSamples, 0, "msaaSamples")
            }
            compare(common_init.theme.type, Theme3D.ThemeUserDefined, "theme")
            compare(common_init.renderingMode, AbstractGraph3D.RenderIndirect, "renderingMode")
            compare(common_init.measureFps, true, "measureFps")
            compare(common_init.customItemList.length, 0, "customItemList")
            compare(common_init.orthoProjection, false, "orthoProjection")
            compare(common_init.aspectRatio, 3.0, "aspectRatio")
            compare(common_init.optimizationHints, AbstractGraph3D.OptimizationStatic, "optimizationHints")
            compare(common_init.polar, false, "polar")
            compare(common_init.radialLabelOffset, 2, "radialLabelOffset")
            compare(common_init.horizontalAspectRatio, 0.2, "horizontalAspectRatio")
            compare(common_init.reflection, true, "reflection")
            compare(common_init.reflectivity, 0.1, "reflectivity")
            compare(common_init.locale, Qt.locale("UK"), "locale")
            compare(common_init.margin, 0.2, "margin")

            waitForRendering(top)
            common_init.destroy();
            waitForRendering(top)
        }
    }
}
