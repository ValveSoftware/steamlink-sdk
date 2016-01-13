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
        id: imageSource1
        source: "images/bug.jpg"
    }

    ImageSource {
        id: imageSource2
        source: "images/butterfly.png"
    }

    Blend {
        id: effect
        anchors.fill: imageSource1
        visible: enabledCheckBox.selected
        cached: cachedCheckBox.selected
        source: imageSource1
        foregroundSource: imageSource2
        mode: blendMode.value
    }

    bgColor: bgColorPicker.color
    controls: [
        Control {
            caption: "general"

            RadioButtonColumn {
                id: blendMode
                value: "normal"
                caption: "mode"
                RadioButton {
                    caption: "normal"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
                RadioButton {
                    caption: "addition"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
                RadioButton {
                    caption: "average"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
                RadioButton {
                    caption: "color"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
                RadioButton {
                    caption: "colorBurn"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
                RadioButton {
                    caption: "colorDodge"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
                RadioButton {
                    caption: "darken"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
                RadioButton {
                    caption: "darkerColor"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
                RadioButton {
                    caption: "difference"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
                RadioButton {
                    caption: "divide"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
                RadioButton {
                    caption: "exclusion"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
                RadioButton {
                    caption: "hardLight"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
                RadioButton {
                    caption: "hue"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
                RadioButton {
                    caption: "lighten"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
                RadioButton {
                    caption: "lighterColor"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
                RadioButton {
                    caption: "lightness"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
                RadioButton {
                    caption: "multiply"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
                RadioButton {
                    caption: "negation"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
                RadioButton {
                    caption: "saturation"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
                RadioButton {
                    caption: "screen"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
                RadioButton {
                    caption: "softlight"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
                RadioButton {
                    caption: "subtract"
                    selected: caption == blendMode.value
                    onPressedChanged: blendMode.value = caption
                }
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
