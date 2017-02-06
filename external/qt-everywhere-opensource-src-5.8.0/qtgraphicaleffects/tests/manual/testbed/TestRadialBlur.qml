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
    ImageSource {
        id: imageSource
        source: "images/butterfly.png"
    }

    RadialBlur {
        id: effect
        anchors.fill: imageSource
        transparentBorder: transparentBorderCheckBox.selected
        angle: lengthSlider.value
        samples: samplesSlider.value
        horizontalOffset: (offsetPicker.xValue - 0.5) * width
        verticalOffset: (offsetPicker.yValue - 0.5) * height
        visible: enabledCheckBox.selected
        cached: cachedCheckBox.selected
        source: imageSource
    }

    PositionPicker {
        id: offsetPicker
        xValue: 0.5
        yValue: 0.5
    }

    bgColor: bgColorPicker.color
    controls: [
        Control {
            caption: "general"
            Slider {
                id: lengthSlider
                minimum: 0.0
                maximum: 360.0
                value: 15.0
                caption: "angle"
            }
            Slider {
                id: samplesSlider
                minimum: 0
                maximum: 64
                value: 32
                caption: "samples"
                integer: true
            }
            CheckBox {
                id: transparentBorderCheckBox
                caption: "transparentBorder"
                selected: false
            }
            Label {
                caption: "horizontalOffset"
                text: effect.horizontalOffset.toFixed(1)
            }
            Label {
                caption: "verticalOffset"
                text: effect.verticalOffset.toFixed(1)
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
