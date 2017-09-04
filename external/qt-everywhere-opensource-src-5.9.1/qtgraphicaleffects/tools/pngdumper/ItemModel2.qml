/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Graphical Effects module.
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

import QtQuick 2.0
import QtGraphicalEffects 1.0

VisualItemModel {

    FastBlur {
        function init() { checkerboard = true }
        width: size
        height: size
        source: bug
        radius: 64
        property string __name: "FastBlur"
        property variant __properties: ["radius", "transparentBorder"]
        property string __varyingProperty: "transparentBorder"
        property variant __values: [false, true]
        function uninit() { checkerboard = false }
    }

    FastBlur {
        width: size
        height: size
        source: bug
        radius: 0
        property string __name: "FastBlur"
        property variant __properties: ["radius"]
        property string __varyingProperty: "radius"
        property variant __values: ["0.0", "32", "64"]
    }

    GammaAdjust {
        width: size
        height: size
        source: bug
        property string __name: "GammaAdjust"
        property variant __properties: ["gamma"]
        property string __varyingProperty: "gamma"
        property variant __values: ["0.5", "1.0", "2.0"]
    }

    GaussianBlur {
        width: size
        height: size
        source: bug
        samples: 16
        deviation: 3
        property string __name: "GaussianBlur"
        property variant __properties: ["radius", "samples", "deviation"]
        property string __varyingProperty: "radius"
        property variant __values: [0, 4, 8]
    }
    GaussianBlur {
        width: size
        height: size
        source: bug
        samples: 16
        radius: 8
        property string __name: "GaussianBlur"
        property variant __properties: ["radius", "samples", "deviation"]
        property string __varyingProperty: "deviation"
        property variant __values: [1, 2, 4]
    }
    GaussianBlur {
        function init() { checkerboard = true }
        width: size
        height: size
        source: bug
        samples: 16
        radius: 8
        property string __name: "GaussianBlur"
        property variant __properties: ["radius", "samples", "deviation", "transparentBorder"]
        property string __varyingProperty: "transparentBorder"
        property variant __values: [false, true]
        function uninit() { checkerboard = false }
    }

    HueSaturation {
        width: size
        height: size
        source: bug
        property string __name: "HueSaturation"
        property variant __properties: ["hue", "saturation", "lightness"]
        property string __varyingProperty: "hue"
        property variant __values: ["-0.3", "0.0", "0.3"]
    }
    HueSaturation {
        width: size
        height: size
        source: bug
        property string __name: "HueSaturation"
        property variant __properties: ["hue", "saturation", "lightness"]
        property string __varyingProperty: "saturation"
        property variant __values: ["-0.8", "0.0", "1.0"]
    }
    HueSaturation {
        width: size
        height: size
        source: bug
        property string __name: "HueSaturation"
        property variant __properties: ["hue", "saturation", "lightness"]
        property string __varyingProperty: "lightness"
        property variant __values: ["-0.5", "0.0", "0.5"]
    }

    InnerShadow {
        function init() {
            background = "white"
        }
        width: size
        height: size
        source: butterfly
        horizontalOffset: 0
        verticalOffset: 0
        samples: 24
        property string __name: "InnerShadow"
        property variant __properties: ["radius", "samples", "color", "horizontalOffset", "verticalOffset", "spread"]
        property string __varyingProperty: "radius"
        property variant __values: [0, 6, 12]
    }
    InnerShadow {
        width: size
        height: size
        source: butterfly
        horizontalOffset: 0
        verticalOffset: 0
        radius: 16
        samples: 24
        property string __name: "InnerShadow"
        property variant __properties: ["radius", "samples", "color", "horizontalOffset", "verticalOffset", "spread"]
        property string __varyingProperty: "horizontalOffset"
        property variant __values: [-20, 0, 20]
    }
    InnerShadow {
        width: size
        height: size
        source: butterfly
        horizontalOffset: 0
        verticalOffset: 0
        radius: 16
        samples: 24
        property string __name: "InnerShadow"
        property variant __properties: ["radius", "samples", "color", "horizontalOffset", "verticalOffset", "spread"]
        property string __varyingProperty: "verticalOffset"
        property variant __values: [-20, 0, 20]
    }
    InnerShadow {
        width: size
        height: size
        source: butterfly
        horizontalOffset: 0
        verticalOffset: 0
        radius: 16
        samples: 24
        property string __name: "InnerShadow"
        property variant __properties: ["radius", "samples", "color", "horizontalOffset", "verticalOffset", "spread"]
        property string __varyingProperty: "spread"
        property variant __values: ["0.0", "0.3", "0.5"]
    }
    InnerShadow {
        width: size
        height: size
        source: butterfly
        horizontalOffset: 0
        verticalOffset: 0
        radius: 16
        spread: 0.2
        samples: 24
        property string __name: "InnerShadow"
        property variant __properties: ["radius", "samples", "color", "horizontalOffset", "verticalOffset", "spread", "fast"]
        property string __varyingProperty: "fast"
        property variant __values: [false, true]
    }
    InnerShadow {
        function init() { checkerboard = true }
        width: size
        height: size
        source: butterfly
        horizontalOffset: 0
        verticalOffset: 0
        radius: 16
        spread: 0.2
        samples: 24
        property string __name: "InnerShadow"
        property variant __properties: ["radius", "samples", "color", "horizontalOffset", "verticalOffset", "spread"]
        property string __varyingProperty: "color"
        property variant __values: ["#000000", "#ffffff", "#ff0000"]
    }

    LinearGradient {
        function init() { checkerboard = true }

        width: size
        height: size
        start: Qt.point(0,0)
        end: Qt.point(width, height)
        property string __name: "LinearGradient"
        property variant __properties: ["start", "end", "gradient"]
        property string __varyingProperty: "gradient"
        property variant __values: [firstGradient, secondGradient, thirdGradient]
    }
    LinearGradient {
        width: size
        height: size
        end: Qt.point(width, height)
        property string __name: "LinearGradient"
        property variant __properties: ["start", "end", "gradient"]
        property string __varyingProperty: "start"
        property variant __values: [Qt.point(0,0), Qt.point(width / 2, height / 2), Qt.point(width, 0)]
    }
    LinearGradient {
        width: size
        height: size
        start: Qt.point(0,0)
        property string __name: "LinearGradient"
        property variant __properties: ["start", "end", "gradient"]
        property string __varyingProperty: "end"
        property variant __values: [Qt.point(width, height), Qt.point(width / 2, height / 2), Qt.point(width, 0)]
    }
    LinearGradient {
        width: size
        height: size
        start: Qt.point(0,0)
        end: Qt.point(width, height)
        property string __name: "LinearGradient"
        property variant __properties: ["start", "end", "gradient", "source"]
        property string __varyingProperty: "source"
        property variant __values: [undefined, butterfly]
    }

    OpacityMask {
        width: size
        height: size
        source: bug
        property string __name: "OpacityMask"
        property variant __properties: ["maskSource"]
        property string __varyingProperty: "maskSource"
        property variant __values: [butterfly]
    }

    RadialGradient {
        width: size
        height: size
        property string __name: "RadialGradient"
        property variant __properties: ["horizontalOffset", "verticalOffset", "horizontalRadius", "verticalRadius", "angle", "gradient"]
        property string __varyingProperty: "horizontalOffset"
        property variant __values: [-width / 2, 0, width / 2]
    }
    RadialGradient {
        width: size
        height: size
        property string __name: "RadialGradient"
        property variant __properties: ["horizontalOffset", "verticalOffset", "horizontalRadius", "verticalRadius", "angle", "gradient"]
        property string __varyingProperty: "verticalOffset"
        property variant __values: [-height / 2, 0, height / 2]
    }
    RadialGradient {
        width: size
        height: size
        property string __name: "RadialGradient"
        property variant __properties: ["horizontalOffset", "verticalOffset", "horizontalRadius", "verticalRadius", "angle", "gradient"]
        property string __varyingProperty: "horizontalRadius"
        property variant __values: [width, width / 3]
    }
    RadialGradient {
        width: size
        height: size
        property string __name: "RadialGradient"
        property variant __properties: ["horizontalOffset", "verticalOffset", "horizontalRadius", "verticalRadius", "angle", "gradient"]
        property string __varyingProperty: "verticalRadius"
        property variant __values: [height, height / 3]
    }
    RadialGradient {
        width: size
        height: size
        property string __name: "RadialGradient"
        property variant __properties: ["horizontalOffset", "verticalOffset", "horizontalRadius", "verticalRadius", "angle", "gradient"]
        property string __varyingProperty: "gradient"
        property variant __values: [firstGradient, secondGradient, thirdGradient]
    }
    RadialGradient {
        width: size
        height: size
        horizontalRadius: width / 3
        property string __name: "RadialGradient"
        property variant __properties: ["horizontalOffset", "verticalOffset", "horizontalRadius", "verticalRadius", "angle", "gradient"]
        property string __varyingProperty: "angle"
        property variant __values: [0, 45, 90]
    }
    RadialGradient {
        width: size
        height: size
        property string __name: "RadialGradient"
        property variant __properties: ["horizontalOffset", "verticalOffset", "horizontalRadius", "verticalRadius", "angle", "gradient", "source"]
        property string __varyingProperty: "source"
        property variant __values: [undefined, butterfly]

        function uninit() { checkerboard = false }
    }

    RectangularGlow {
        function init() {
            background = "black"
            rect.visible = true
        }
        width: rect.width
        height: rect.height
        x: rect.x
        property string __name: "RectangularGlow"
        property variant __properties: ["glowRadius", "spread", "color", "cornerRadius"]
        property string __varyingProperty: "glowRadius"
        property variant __values: [10, 20, 40]
        cornerRadius: 25
    }
    RectangularGlow {
        width: rect.width
        height: rect.height
        x: rect.x
        property string __name: "RectangularGlow"
        property variant __properties: ["glowRadius", "spread", "color", "cornerRadius"]
        property string __varyingProperty: "spread"
        property variant __values: ["0.0", "0.5", "1.0"]
        cornerRadius: 25
        glowRadius: 20
    }
    RectangularGlow {
        width: rect.width
        height: rect.height
        x: rect.x
        property string __name: "RectangularGlow"
        property variant __properties: ["glowRadius", "spread", "color", "cornerRadius"]
        property string __varyingProperty: "color"
        property variant __values: ["#ffffff", "#55ff55", "#5555ff"]
        cornerRadius: 25
        glowRadius: 20
    }
    RectangularGlow {
        width: rect.width
        height: rect.height
        x: rect.x
        property string __name: "RectangularGlow"
        property variant __properties: ["glowRadius", "spread", "color", "cornerRadius"]
        property string __varyingProperty: "cornerRadius"
        property variant __values: [0, 25, 50]
        glowRadius: 20

        function uninit() {
            background = "white"
            rect.visible = false
        }
    }

    Item {
        id: theEnd
        width: size
        height: size
        function init() { Qt.quit() }
    }
}
