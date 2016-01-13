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
import "../../../src/effects"

TestCaseTemplate {
    ImageSource {
        id: maskImage
        source: "images/butterfly.png"
    }

    LinearGradient {
        id: effect
        anchors.fill: parent
        cached: cachedCheckBox.selected
        visible: enabledCheckBox.selected
        source: maskCheckBox.selected ? maskImage : undefined
        start: Qt.point(startPicker.xValue * width, startPicker.yValue * height)
        end: Qt.point(endPicker.xValue * width, endPicker.yValue * height)
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: gradientBeginColorSlider.color
            }
            GradientStop {
                position: 1.0
                color: gradientEndColorSlider.color
            }
        }
    }

    PositionPicker {
        id: startPicker
        xValue: 0.2
        yValue: 0.2
    }
    PositionPicker {
        id: endPicker
        xValue: 0.8
        yValue: 0.8
    }

    bgColor: bgColorPicker.color
    controls: [
        Control {
            caption: "general"
            Label {
                caption: "startX"
                text: startPicker.xValue.toFixed(1)
            }
            Label {
                caption: "startY"
                text: startPicker.yValue.toFixed(1)
            }
            Label {
                caption: "endX"
                text: endPicker.xValue.toFixed(1)
            }
            Label {
                caption: "endY"
                text: endPicker.yValue.toFixed(1)
            }
            CheckBox {
                id: maskCheckBox
                caption: "Use Mask"
            }
        },

        Control {
            caption: "gradient begin color"
            ColorPicker {
                id: gradientBeginColorSlider
                hue: 0.67
                saturation: 1.0
                lightness: 0.5
            }
        },

        Control {
            caption: "gradient end color"
            ColorPicker {
                id: gradientEndColorSlider
                hue: 0.5
                saturation: 1.0
                lightness: 0.5
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
            CheckBox {
                id: updateCheckBox
                caption: "animated"
                selected: false
            }
            BGColorPicker {
                id: bgColorPicker
            }
        }
    ]
}
