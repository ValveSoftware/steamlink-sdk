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

    Theme3D {
        id: initial
    }

    ColorGradient {
        id: gradient1
        stops: [
            ColorGradientStop { color: "red"; position: 0 },
            ColorGradientStop { color: "blue"; position: 1 }
        ]
    }

    ColorGradient {
        id: gradient2
        stops: [
            ColorGradientStop { color: "green"; position: 0 },
            ColorGradientStop { color: "red"; position: 1 }
        ]
    }

    ColorGradient {
        id: gradient3
        stops: [
            ColorGradientStop { color: "gray"; position: 0 },
            ColorGradientStop { color: "darkgray"; position: 1 }
        ]
    }

    ThemeColor {
        id: color1
        color: "red"
    }

    ThemeColor {
        id: color2
        color: "blue"
    }

    Theme3D {
        id: initialized
        ambientLightStrength: 0.3
        backgroundColor: "#ff0000"
        backgroundEnabled: false
        baseColors: [color1, color2]
        baseGradients: [gradient1, gradient2]
        colorStyle: Theme3D.ColorStyleRangeGradient
        font.family: "Arial"
        gridEnabled: false
        gridLineColor: "#00ff00"
        highlightLightStrength: 5.0
        labelBackgroundColor: "#ff00ff"
        labelBackgroundEnabled: false
        labelBorderEnabled: false
        labelTextColor: "#00ffff"
        lightColor: "#ffff00"
        lightStrength: 2.5
        multiHighlightColor: "#ff00ff"
        multiHighlightGradient: gradient3
        singleHighlightColor: "#ff0000"
        singleHighlightGradient: gradient3
        type: Theme3D.ThemeQt // Default values will be overwritten by initialized values
        windowColor: "#fff00f"
    }

    Theme3D {
        id: change
    }

    Theme3D {
        id: invalid
    }

    TestCase {
        name: "Theme3D Initial"

        Text { id: dummy }

        function test_initial() {
            compare(initial.ambientLightStrength, 0.25)
            compare(initial.backgroundColor, "#000000")
            compare(initial.backgroundEnabled, true)
            compare(initial.baseColors.length, 1)
            compare(initial.baseColors[0].color, "#000000")
            compare(initial.baseGradients.length, 1)
            compare(initial.baseGradients[0].stops[0].color, "#000000")
            compare(initial.baseGradients[0].stops[1].color, "#ffffff")
            compare(initial.colorStyle, Theme3D.ColorStyleUniform)
            // Initial font needs to be tested like this, as different platforms have different default font (QFont())
            compare(initial.font.family, dummy.font.family)
            compare(initial.gridEnabled, true)
            compare(initial.gridLineColor, "#ffffff")
            compare(initial.highlightLightStrength, 7.5)
            compare(initial.labelBackgroundColor, "#a0a0a4")
            compare(initial.labelBackgroundEnabled, true)
            compare(initial.labelBorderEnabled, true)
            compare(initial.labelTextColor, "#ffffff")
            compare(initial.lightColor, "#ffffff")
            compare(initial.lightStrength, 5)
            compare(initial.multiHighlightColor, "#0000ff")
            compare(initial.multiHighlightGradient, null)
            compare(initial.singleHighlightColor, "#ff0000")
            compare(initial.singleHighlightGradient, null)
            compare(initial.type, Theme3D.ThemeUserDefined)
            compare(initial.windowColor, "#000000")
        }
    }

    TestCase {
        name: "Theme3D Initialized"

        function test_initialized() {
            compare(initialized.ambientLightStrength, 0.3)
            compare(initialized.backgroundColor, "#ff0000")
            compare(initialized.backgroundEnabled, false)
            compare(initialized.baseColors.length, 2)
            compare(initialized.baseColors[0].color, "#ff0000")
            compare(initialized.baseColors[1].color, "#0000ff")
            compare(initialized.baseGradients.length, 2)
            compare(initialized.baseGradients[0], gradient1)
            compare(initialized.baseGradients[1], gradient2)
            compare(initialized.colorStyle, Theme3D.ColorStyleRangeGradient)
            compare(initialized.font.family, "Arial")
            compare(initialized.gridEnabled, false)
            compare(initialized.gridLineColor, "#00ff00")
            compare(initialized.highlightLightStrength, 5.0)
            compare(initialized.labelBackgroundColor, "#ff00ff")
            compare(initialized.labelBackgroundEnabled, false)
            compare(initialized.labelBorderEnabled, false)
            compare(initialized.labelTextColor, "#00ffff")
            compare(initialized.lightColor, "#ffff00")
            compare(initialized.lightStrength, 2.5)
            compare(initialized.multiHighlightColor, "#ff00ff")
            compare(initialized.multiHighlightGradient, gradient3)
            compare(initialized.singleHighlightColor, "#ff0000")
            compare(initialized.singleHighlightGradient, gradient3)
            compare(initialized.type, Theme3D.ThemeQt)
            compare(initialized.windowColor, "#fff00f")
        }
    }

    TestCase {
        name: "Theme3D Change"

        ThemeColor {
            id: color3
            color: "red"
        }

        ColorGradient {
            id: gradient4
            stops: [
                ColorGradientStop { color: "red"; position: 0 },
                ColorGradientStop { color: "blue"; position: 1 }
            ]
        }

        function test_1_change() {
            change.type = Theme3D.ThemeStoneMoss // Default values will be overwritten by the following sets
            change.ambientLightStrength = 0.3
            change.backgroundColor = "#ff0000"
            change.backgroundEnabled = false
            change.baseColors = [color3, color2]
            change.baseGradients = [gradient4, gradient2]
            change.colorStyle = Theme3D.ColorStyleObjectGradient
            change.font.family = "Arial"
            change.gridEnabled = false
            change.gridLineColor = "#00ff00"
            change.highlightLightStrength = 5.0
            change.labelBackgroundColor = "#ff00ff"
            change.labelBackgroundEnabled = false
            change.labelBorderEnabled = false
            change.labelTextColor = "#00ffff"
            change.lightColor = "#ffff00"
            change.lightStrength = 2.5
            change.multiHighlightColor = "#ff00ff"
            change.multiHighlightGradient = gradient3
            change.singleHighlightColor = "#ff0000"
            change.singleHighlightGradient = gradient3
            change.windowColor = "#fff00f"

            compare(change.ambientLightStrength, 0.3)
            compare(change.backgroundColor, "#ff0000")
            compare(change.backgroundEnabled, false)
            compare(change.baseColors.length, 2)
            compare(change.baseColors[0].color, "#ff0000")
            compare(change.baseColors[1].color, "#0000ff")
            compare(change.baseGradients.length, 2)
            compare(change.baseGradients[0], gradient4)
            compare(change.baseGradients[1], gradient2)
            compare(change.colorStyle, Theme3D.ColorStyleObjectGradient)
            compare(change.font.family, "Arial")
            compare(change.gridEnabled, false)
            compare(change.gridLineColor, "#00ff00")
            compare(change.highlightLightStrength, 5.0)
            compare(change.labelBackgroundColor, "#ff00ff")
            compare(change.labelBackgroundEnabled, false)
            compare(change.labelBorderEnabled, false)
            compare(change.labelTextColor, "#00ffff")
            compare(change.lightColor, "#ffff00")
            compare(change.lightStrength, 2.5)
            compare(change.multiHighlightColor, "#ff00ff")
            compare(change.multiHighlightGradient, gradient3)
            compare(change.singleHighlightColor, "#ff0000")
            compare(change.singleHighlightGradient, gradient3)
            compare(change.type, Theme3D.ThemeStoneMoss)
            compare(change.windowColor, "#fff00f")
        }

        function test_2_change_color() {
            color3.color = "white"
            compare(change.baseColors[0].color, "#ffffff")
        }

        function test_3_change_gradient() {
            gradient4.stops[0].color = "black"
            compare(change.baseGradients[0].stops[0].color, "#000000")
        }
    }


    TestCase {
        name: "Theme3D Invalid"

        function test_invalid() {
            invalid.ambientLightStrength = -1.0
            compare(invalid.ambientLightStrength, 0.25)
            invalid.ambientLightStrength = 1.1
            compare(invalid.ambientLightStrength, 0.25)
            invalid.highlightLightStrength = -1.0
            compare(invalid.highlightLightStrength, 7.5)
            invalid.highlightLightStrength = 10.1
            compare(invalid.highlightLightStrength, 7.5)
            invalid.lightStrength = -1.0
            compare(invalid.lightStrength, 5.0)
            invalid.lightStrength = 10.1
            compare(invalid.lightStrength, 5.0)
        }
    }
}
