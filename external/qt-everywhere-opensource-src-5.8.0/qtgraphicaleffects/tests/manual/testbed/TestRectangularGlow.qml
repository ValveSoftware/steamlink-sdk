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
import "../../../src/effects"

TestCaseTemplate {

    RectangularGlow {
        id: effect
        anchors.fill: rectSource
        color: colorPicker.color
        spread: spreadSlider.value
        glowRadius: sizeSlider.value
        visible: enabledCheckBox.selected
        opacity: opacitySlider.value
        cornerRadius: radiusSlider.value
        cached: cachedCheckBox.selected
    }

    Rectangle {
        id: rectSource
        visible: true
        anchors.centerIn: parent
        width: Math.round(parent.width / 1.5)
        height: Math.round(parent.height / 2)
        radius: 25
        color: "black"
        smooth: true
    }

    bgColor: bgColorPicker.color
    controls: [
        Control {
            caption: "general"
            Slider {
                id: spreadSlider
                caption: "spread"
                minimum: 0.0
                maximum: 1.0
                value: 0.5
            }
            Slider {
                id: sizeSlider
                minimum: 0
                maximum: rectSource.width / 2.0
                value: 10.0
                caption: "glowRadius"
                onValueChanged: radiusSlider.value = Math.max(0, Math.min(radiusSlider.value, radiusSlider.maximum))
            }
            Slider {
                id: radiusSlider
                minimum: 0
                maximum: Math.min(effect.width, effect.height) / 2 + effect.glowRadius;
                caption: "cornerRadius"
                value: rectSource.radius + effect.glowRadius
            }
            Slider {
                id: opacitySlider
                minimum: 0
                maximum: 1.0
                value: 1.0
                caption: "opacity"
            }
        },

        Control {
            caption: "color"
            ColorPicker {
                id: colorPicker
                hue: 0
                saturation: 1
                lightness: 1
            }
        },

        Control {
            caption: "advanced"
            last: true
            Label {
                caption: "Effect size"
                text: effect.width + "x" + effect.height
            }
            Label {
                caption: "FPS"
                text: fps
            }
            CheckBox {
                id: cachedCheckBox
                caption: "cached"
            }
            CheckBox {
                id: enabledCheckBox
                caption: "enabled"
            }
            BGColorPicker {
                id: bgColorPicker
            }
        }
    ]
}
