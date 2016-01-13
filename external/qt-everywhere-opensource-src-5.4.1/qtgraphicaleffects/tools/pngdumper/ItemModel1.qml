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
    Blend {
        width: size
        height: size
        source: bug
        foregroundSource: butterfly
        mode: "normal"
        property string __name: "Blend"
        property variant __properties: ["mode"]
        property string __varyingProperty: "mode"
        property variant __values: ["normal", "addition","average", "color", "colorBurn","colorDodge", "darken", "darkerColor", "difference", "divide",
           "exclusion", "hardlight", "hue", "lighten", "lighterColor", "lightness", "negation", "multiply", "saturation", "screen", "subtract", "softLight"]
    }

    BrightnessContrast {
        width: size
        height: size
        source: bug
        property string __name: "BrightnessContrast"
        property variant __properties: ["brightness", "contrast"]
        property string __varyingProperty: "brightness"
        property variant __values: [-0.25, 0.0, 0.5]
    }
    BrightnessContrast {
        width: size
        height: size
        source: bug
        property string __name: "BrightnessContrast"
        property variant __properties: ["brightness", "contrast"]
        property string __varyingProperty: "contrast"
        property variant __values: [-0.5, 0.0, 0.5]
    }

    Colorize {
        width: size
        height: size
        source: bug
        property string __name: "Colorize"
        property variant __properties: ["hue", "saturation", "lightness"]
        property string __varyingProperty: "hue"
        property variant __values: [0.2, 0.5, 0.8]
    }
    Colorize {
        width: size
        height: size
        source: bug
        property string __name: "Colorize"
        property variant __properties: ["hue", "saturation", "lightness"]
        property string __varyingProperty: "saturation"
        property variant __values: [0.0, 0.5, 1.0]
    }
    Colorize {
        width: size
        height: size
        source: bug
        property string __name: "Colorize"
        property variant __properties: ["hue", "saturation", "lightness"]
        property string __varyingProperty: "lightness"
        property variant __values: [-0.75, 0.0, 0.75]
    }

    ColorOverlay {
        width: size
        height: size
        source: bug
        property string __name: "ColorOverlay"
        property variant __properties: ["color"]
        property string __varyingProperty: "color"
        property variant __values: ["#80ff0000", "#8000ff00", "#800000ff"]
    }

    ConicalGradient {
        function init() { checkerboard = true }

        width: size
        height: size
        property string __name: "ConicalGradient"
        property variant __properties: ["angle", "horizontalOffset", "verticalOffset", "gradient"]
        property string __varyingProperty: "angle"
        property variant __values: [0, 45, 185]
    }
    ConicalGradient {
        width: size
        height: size
        property string __name: "ConicalGradient"
        property variant __properties: ["angle", "horizontalOffset", "verticalOffset", "gradient"]
        property string __varyingProperty: "horizontalOffset"
        property variant __values: [-50, 0, 50]
    }
    ConicalGradient {
        width: size
        height: size
        property string __name: "ConicalGradient"
        property variant __properties: ["angle", "horizontalOffset", "verticalOffset", "gradient"]
        property string __varyingProperty: "verticalOffset"
        property variant __values: [-50, 0, 50]
    }
    ConicalGradient {
        width: size
        height: size
        property string __name: "ConicalGradient"
        property variant __properties: ["angle", "horizontalOffset", "verticalOffset", "gradient"]
        property string __varyingProperty: "gradient"
        property variant __values: [firstGradient, secondGradient, thirdGradient]
    }
    ConicalGradient {
        width: size
        height: size
        property string __name: "ConicalGradient"
        property variant __properties: ["angle", "horizontalOffset", "verticalOffset", "gradient", "source"]
        property string __varyingProperty: "source"
        property variant __values: [undefined, butterfly]

        function uninit() { checkerboard = false }
    }

    Desaturate {
        width: size
        height: size
        source: butterfly
        property string __name: "Desaturate"
        property variant __properties: ["desaturation"]
        property string __varyingProperty: "desaturation"
        property variant __values: ["0.0", "0.5", "1.0"]
    }

    Displace {
        width: size
        height: size
        source: bug
        displacementSource: ShaderEffectSource {sourceItem: displacementMapSource; visible: false }
        property string __name: "Displace"
        property variant __properties: ["displacement"]
        property string __varyingProperty: "displacement"
        property variant __values: ["-0.2", "0.0", "0.2"]
    }

    DropShadow {
        function init() { butterfly.visible = true }

        width: size
        height: size
        source: butterfly
        horizontalOffset: 0
        verticalOffset: 20
        samples: 16
        property string __name: "DropShadow"
        property variant __properties: ["radius", "samples", "color", "horizontalOffset", "verticalOffset", "spread"]
        property string __varyingProperty: "radius"
        property variant __values: [0, 6, 12]
    }
    DropShadow {
        function init() { checkerboard = true }
        width: size
        height: size
        source: butterfly
        horizontalOffset: 0
        verticalOffset: 20
        radius: 8
        samples: 16
        property string __name: "DropShadow"
        property variant __properties: ["radius", "samples", "color", "horizontalOffset", "verticalOffset", "spread"]
        property string __varyingProperty: "color"
        property variant __values: ["#000000", "#0000ff", "#aa000000"]

        function uninit() { checkerboard = false }
    }
    DropShadow {
        width: size
        height: size
        source: butterfly
        //verticalOffset: 3
        radius: 4
        samples: 16
        property string __name: "DropShadow"
        property variant __properties: ["radius", "samples", "color", "horizontalOffset", "verticalOffset", "spread"]
        property string __varyingProperty: "horizontalOffset"
        property variant __values: ["-20", "0", "20"]
    }
    DropShadow {
        width: size
        height: size
        source: butterfly
        //horizontalOffset: 3
        radius: 4
        samples: 16
        property string __name: "DropShadow"
        property variant __properties: ["radius", "samples", "color", "horizontalOffset", "verticalOffset", "spread"]
        property string __varyingProperty: "verticalOffset"
        property variant __values: ["-20", "0", "20"]
    }
    DropShadow {
        width: size
        height: size
        source: butterfly
        //horizontalOffset: 3
        verticalOffset: 20
        radius: 8
        samples: 16
        property string __name: "DropShadow"
        property variant __properties: ["radius", "samples", "color", "horizontalOffset", "verticalOffset", "spread"]
        property string __varyingProperty: "spread"
        property variant __values: ["0.0", "0.5", "1.0"]
    }
    DropShadow {
        width: size
        height: size
        source: butterfly
        //horizontalOffset: 3
        verticalOffset: 20
        radius: 16
        samples: 24
        property string __name: "DropShadow"
        property variant __properties: ["radius", "samples", "color", "horizontalOffset", "verticalOffset", "spread", "fast"]
        property string __varyingProperty: "fast"
        property variant __values: [false, true]
    }

    Glow {
        function init() { background = "black" }

        width: size
        height: size
        source: butterfly
        samples: 16
        property string __name: "Glow"
        property variant __properties: ["radius", "samples", "color", "spread"]
        property string __varyingProperty: "radius"
        property variant __values: [0, 6, 12]
    }
    Glow {
        width: size
        height: size
        source: butterfly
        radius: 8
        spread: 0.5
        samples: 16
        property string __name: "Glow"
        property variant __properties: ["radius", "samples", "color", "spread"]
        property string __varyingProperty: "color"
        property variant __values: ["#ffffff", "#00ff00", "#aa00ff00"]
    }
    Glow {
        width: size
        height: size
        source: butterfly
        radius: 8
        samples: 16
        property string __name: "Glow"
        property variant __properties: ["radius", "samples", "color", "spread"]
        property string __varyingProperty: "spread"
        property variant __values: ["0.0", "0.5", "1.0"]
    }
    Glow {
        width: size
        height: size
        source: butterfly
        radius: 16
        samples: 24
        spread: 0.3
        property string __name: "Glow"
        property variant __properties: ["radius", "samples", "color", "spread", "fast"]
        property string __varyingProperty: "fast"
        property variant __values: [false, true]

        function uninit() { butterfly.visible = false }
    }


    Item {
        id: theEnd
        width: size
        height: size
        function init() { Qt.quit() }
    }
}
