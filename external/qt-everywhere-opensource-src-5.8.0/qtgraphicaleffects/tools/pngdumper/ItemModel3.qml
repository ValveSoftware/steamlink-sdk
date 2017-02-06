/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
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
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
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

    RecursiveBlur {
        function init() {
            timer.interval = 2000
            checkerboard = true
            }

        width: size
        height: size
        source: bug
        radius: 7.5
        property string __name: "RecursiveBlur"
        property variant __properties: ["loops", "radius"]
        property string __varyingProperty: "loops"
        property variant __values: [4, 20, 70]
    }
    RecursiveBlur {
        width: size
        height: size
        source: bug
        loops: 20
        property string __name: "RecursiveBlur"
        property variant __properties: ["loops", "radius"]
        property string __varyingProperty: "radius"
        property variant __values: [2.5, 4.5, 7.5]
    }
    RecursiveBlur {
        width: size
        height: size
        source: bug
        loops: 20
        radius: 7.5
        property string __name: "RecursiveBlur"
        property variant __properties: ["loops", "radius", "transparentBorder"]
        property string __varyingProperty: "transparentBorder"
        property variant __values: [false, true]

        function uninit() {
            timer.interval = timerInterval
            checkerboard = false
        }
    }

    ThresholdMask {
        width: size
        height: size
        source: bug
        maskSource: fog
        threshold: 0.4
        property string __name: "ThresholdMask"
        property variant __properties: ["spread", "threshold"]
        property string __varyingProperty: "spread"
        property variant __values: ["0.0", "0.2", "0.8"]
        function init() { checkerboard = true }
    }
    ThresholdMask {
        width: size
        height: size
        source: bug
        maskSource: fog
        spread: 0.2
        property string __name: "ThresholdMask"
        property variant __properties: ["spread", "threshold"]
        property string __varyingProperty: "threshold"
        property variant __values: ["0.0", "0.5", "0.7"]
        function uninit() { checkerboard = false }
    }

    RadialBlur {
        width: size
        height: size
        source: butterfly
        samples: 24
        property string __name: "RadialBlur"
        property variant __properties: ["samples", "angle", "horizontalOffset", "verticalOffset"]
        property string __varyingProperty: "angle"
        property variant __values: ["0.0", "15.0", "30.0"]
        function uninit() { checkerboard = false }
    }
    RadialBlur {
        width: size
        height: size
        source: butterfly
        samples: 24
        angle: 20
        property string __name: "RadialBlur"
        property variant __properties: ["samples", "angle", "horizontalOffset", "verticalOffset"]
        property string __varyingProperty: "horizontalOffset"
        property variant __values: ["75.0", "0.0", "-75.0"]
        function uninit() { checkerboard = false }
    }
    RadialBlur {
        width: size
        height: size
        source: butterfly
        samples: 24
        angle: 20
        property string __name: "RadialBlur"
        property variant __properties: ["samples", "angle", "horizontalOffset", "verticalOffset"]
        property string __varyingProperty: "verticalOffset"
        property variant __values: ["75.0", "0.0", "-75.0"]
        function uninit() { checkerboard = false }
    }

    DirectionalBlur {
        width: size
        height: size
        source: butterfly
        samples: 24
        length: 32
        property string __name: "DirectionalBlur"
        property variant __properties: ["samples", "angle", "length"]
        property string __varyingProperty: "angle"
        property variant __values: ["0.0", "45.0", "90.0"]
        function uninit() { checkerboard = false }
    }
    DirectionalBlur {
        width: size
        height: size
        source: butterfly
        samples: 24
        property string __name: "DirectionalBlur"
        property variant __properties: ["samples", "angle", "length"]
        property string __varyingProperty: "length"
        property variant __values: ["0.0", "32.0", "48.0"]
        function uninit() { checkerboard = false }
    }

    ZoomBlur {
        width: size
        height: size
        source: butterfly
        samples: 24
        length: 32
        property string __name: "ZoomBlur"
        property variant __properties: ["samples", "length", "horizontalOffset", "verticalOffset"]
        property string __varyingProperty: "horizontalOffset"
        property variant __values: ["100.0", "0.0", "-100.0"]
        function uninit() { checkerboard = false }
    }
    ZoomBlur {
        width: size
        height: size
        source: butterfly
        samples: 24
        length: 32
        property string __name: "ZoomBlur"
        property variant __properties: ["samples", "length", "horizontalOffset", "verticalOffset"]
        property string __varyingProperty: "verticalOffset"
        property variant __values: ["100.0", "0.0", "-100.0"]
        function uninit() { checkerboard = false }
    }
    ZoomBlur {
        width: size
        height: size
        source: butterfly
        samples: 24
        property string __name: "ZoomBlur"
        property variant __properties: ["samples", "length", "horizontalOffset", "verticalOffset"]
        property string __varyingProperty: "length"
        property variant __values: ["0.0", "32.0", "48.0"]
        function uninit() { checkerboard = false }
    }

    LevelAdjust {
        width: size
        height: size
        source: butterfly
        property string __name: "LevelAdjust"
        property variant __properties: ["minimumInput", "maximumInput", "minimumOutput", "maximumOutput", "gamma"]
        property string __varyingProperty: "minimumInput"
        property variant __values: ["#00000000", "#00000040", "#00000070"]
    }

    LevelAdjust {
        width: size
        height: size
        source: butterfly
        property string __name: "LevelAdjust"
        property variant __properties: ["minimumInput", "maximumInput", "minimumOutput", "maximumOutput", "gamma"]
        property string __varyingProperty: "maximumInput"
        property variant __values: ["#FFFFFFFF", "#FFFFFF80", "#FFFFFF30"]
    }

    LevelAdjust {
        width: size
        height: size
        source: butterfly
        property string __name: "LevelAdjust"
        property variant __properties: ["minimumInput", "maximumInput", "minimumOutput", "maximumOutput", "gamma"]
        property string __varyingProperty: "minimumOutput"
        property variant __values: ["#00000000", "#00000070", "#000000A0"]
    }

    LevelAdjust {
        width: size
        height: size
        source: butterfly
        property string __name: "LevelAdjust"
        property variant __properties: ["minimumInput", "maximumInput", "minimumOutput", "maximumOutput", "gamma"]
        property string __varyingProperty: "maximumOutput"
        property variant __values: ["#FFFFFFFF", "#FFFFFF80", "#FFFFFF30"]
    }

    Item {
        id: theEnd
        width: size
        height: size
        function init() { Qt.quit() }
    }
}
