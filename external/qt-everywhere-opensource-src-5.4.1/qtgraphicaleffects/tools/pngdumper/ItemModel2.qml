/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Graphical Effects module.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
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
