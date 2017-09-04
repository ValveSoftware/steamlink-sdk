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
