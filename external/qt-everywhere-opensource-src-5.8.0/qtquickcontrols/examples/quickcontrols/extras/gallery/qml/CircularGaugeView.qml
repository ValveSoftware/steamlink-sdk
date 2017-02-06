/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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

import QtQuick 2.2
import QtQuick.Controls 1.1
import QtQuick.Extras 1.4

ControlView {
    id: controlView
    darkBackground: customizerItem.currentStyleDark

    property color fontColor: darkBackground ? "white" : "black"

    property bool accelerating: false

    Keys.onSpacePressed: accelerating = true
    Keys.onReleased: {
        if (event.key === Qt.Key_Space) {
            accelerating = false;
            event.accepted = true;
        }
    }

    Button {
        id: accelerate
        text: "Accelerate"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        height: root.height * 0.125

        onPressedChanged: accelerating = pressed

        style: BlackButtonStyle {
            background: BlackButtonBackground {
                pressed: control.pressed
            }
            label: Text {
                text: control.text
                color: "white"
                font.pixelSize: Math.max(textSingleton.font.pixelSize, root.toPixels(0.04))
                font.family: openSans.name
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    control: CircularGauge {
        id: gauge
        minimumValue: customizerItem.minimumValue
        maximumValue: customizerItem.maximumValue
        width: controlBounds.width
        height: controlBounds.height

        value: accelerating ? maximumValue : 0
        style: styleMap[customizerItem.currentStylePath]

        // This stops the styles being recreated when a new one is chosen.
        property var styleMap: {
            var styles = {};
            for (var i = 0; i < customizerItem.allStylePaths.length; ++i) {
                var path = customizerItem.allStylePaths[i];
                styles[path] = Qt.createComponent(path, gauge);
            }
            styles;
        }

        // Called to update the style after the user has edited a property.
        Connections {
            target: customizerItem
            onMinimumValueAngleChanged: __style.minimumValueAngle = customizerItem.minimumValueAngle
            onMaximumValueAngleChanged: __style.maximumValueAngle = customizerItem.maximumValueAngle
            onLabelStepSizeChanged: __style.tickmarkStepSize = __style.labelStepSize = customizerItem.labelStepSize
        }

        Behavior on value {
            NumberAnimation {
                easing.type: Easing.OutCubic
                duration: 6000
            }
        }
    }

    customizer: Column {
        readonly property var allStylePaths: {
            var paths = [];
            for (var i = 0; i < stylePicker.model.count; ++i) {
                paths.push(stylePicker.model.get(i).path);
            }
            paths;
        }
        property alias currentStylePath: stylePicker.currentStylePath
        property alias currentStyleDark: stylePicker.currentStyleDark
        property alias minimumValue: minimumValueSlider.value
        property alias maximumValue: maximumValueSlider.value
        property alias minimumValueAngle: minimumAngleSlider.value
        property alias maximumValueAngle: maximumAngleSlider.value
        property alias labelStepSize: labelStepSizeSlider.value

        id: circularGaugeColumn
        spacing: customizerPropertySpacing

        readonly property bool isDefaultStyle: stylePicker.model.get(stylePicker.currentIndex).name === "Default"

        Item {
            id: stylePickerBottomSpacing
            width: parent.width
            height: stylePicker.height + textSingleton.implicitHeight

            StylePicker {
                id: stylePicker
                currentIndex: 1

                model: ListModel {
                    ListElement {
                        name: "Default"
                        path: "CircularGaugeDefaultStyle.qml"
                        dark: true
                    }
                    ListElement {
                        name: "Dark"
                        path: "CircularGaugeDarkStyle.qml"
                        dark: true
                    }
                    ListElement {
                        name: "Light"
                        path: "CircularGaugeLightStyle.qml"
                        dark: false
                    }
                }
            }
        }

        CustomizerLabel {
            text: "Minimum angle"
        }

        CustomizerSlider {
            id: minimumAngleSlider
            minimumValue: -180
            value: -145
            maximumValue: 180
            width: parent.width
        }

        CustomizerLabel {
            text: "Maximum angle"
        }

        CustomizerSlider {
            id: maximumAngleSlider
            minimumValue: -180
            value: 145
            maximumValue: 180
        }

        CustomizerLabel {
            text: "Minimum value"
        }

        CustomizerSlider {
            id: minimumValueSlider
            minimumValue: 0
            value: 0
            maximumValue: 360
            stepSize: 1
        }

        CustomizerLabel {
            text: "Maximum value"
        }

        CustomizerSlider {
            id: maximumValueSlider
            minimumValue: 0
            value: 240
            maximumValue: 300
            stepSize: 1
        }

        CustomizerLabel {
            text: "Label step size"
        }

        CustomizerSlider {
            id: labelStepSizeSlider
            minimumValue: 10
            value: 20
            maximumValue: 100
            stepSize: 20
        }
    }
}
