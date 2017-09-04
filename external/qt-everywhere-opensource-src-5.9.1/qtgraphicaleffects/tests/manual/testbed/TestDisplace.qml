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
import "../../../src/effects"

TestCaseTemplate {

    ImageSource {
        id: imageSource
        source: "images/bug.jpg"
    }

    Rectangle {
        id: displacementMapSource
        visible: !enabledCheckBox.selected
        color: "#8080ff"
        smooth: true
        anchors.fill: parent
        Image {
            id: di
            x: (parent.width - width) / 2
            y: (parent.height - height) / 2
            sourceSize: Qt.size(128, 128)
            source: "images/glass_normal.png"
            smooth: true
        }
    }

    Displace {
        id: effect
        anchors.fill: imageSource
        visible: enabledCheckBox.selected
        cached: cachedCheckBox.selected
        source: imageSource
        displacementSource: displacementMapSource
        displacement: displacementScaleSlider.value
        smooth: true
        MouseArea {
            anchors.fill: parent
            onClicked: { di.x = mouseX - di.width/2; di.y = mouseY - di.height/2; }
            onPositionChanged: { if (pressed) { di.x = mouseX - di.width/2; di.y = mouseY - di.height/2; } }
        }
    }

    bgColor: bgColorPicker.color
    controls: [
        Control {
            caption: "general"
            Slider {
                id: displacementScaleSlider
                caption: "displacement"
                minimum: -0.5
                maximum: 0.5
                value: 0.1
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
            RadioButtonColumn {
                id: sourceType
                value: "shaderEffectSource"
                caption: "source type"
                RadioButton {
                    caption: "shaderEffectSource"
                    selected: caption == sourceType.value
                    onPressedChanged: sourceType.value = caption
                }
                RadioButton {
                    caption: "image"
                    selected: caption == sourceType.value
                    onPressedChanged: {
                        sourceType.value = caption
                        updateCheckBox.selected = false
                    }
                }
            }
            BGColorPicker {
                id: bgColorPicker
            }
        }
    ]
}
