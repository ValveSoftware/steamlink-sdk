/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

import QtQuick 2.7
import QtQuick.Controls 2.0 as QQC2
import Qt.labs.settings 1.0
import "../Style"

Item {

    Settings {
        id: settings
        property alias wireless: wirelessSwitch.checked
        property alias bluetooth: bluetoothSwitch.checked
        property alias contrast: contrastSlider.value
        property alias brightness: brightnessSlider.value
    }

    QQC2.SwipeView {
        id: svSettingsContainer

        anchors.fill: parent

        Item {
            id: settingsPage1

            Column {
                anchors.centerIn: parent
                spacing: 25

                Row {
                    spacing: 50
                    Image {
                        anchors.verticalCenter: parent.verticalCenter
                        source: "images/bluetooth.png"
                    }
                    QQC2.Switch {
                        id: bluetoothSwitch
                        anchors.verticalCenter: parent.verticalCenter
                        checked: settings.bluetooth
                    }
                }
                Row {
                    spacing: 50
                    Image {
                        anchors.verticalCenter: parent.verticalCenter
                        source: "images/wifi.png"
                    }
                    QQC2.Switch {
                        id: wirelessSwitch
                        anchors.verticalCenter: parent.verticalCenter
                        checked: settings.wireless
                    }
                }
            }
        }

        Item {
            id: settingsPage2

            Column {
                anchors.centerIn: parent
                spacing: 2

                Column {
                    Image {
                        anchors.horizontalCenter: parent.horizontalCenter
                        source: "images/brightness.png"
                    }
                    QQC2.Slider {
                        id: brightnessSlider
                        anchors.horizontalCenter: parent.horizontalCenter
                        from: 0
                        to: 5
                        stepSize: 1
                        value: settings.brightness
                    }
                }
                Column {
                    spacing: 2
                    Image {
                        anchors.horizontalCenter: parent.horizontalCenter
                        source: "images/contrast.png"
                    }
                    QQC2.Slider {
                        id: contrastSlider
                        anchors.horizontalCenter: parent.horizontalCenter
                        from: 0
                        to: 10
                        stepSize: 1
                        value: settings.contrast
                    }
                }
            }
        }
    }

    QQC2.PageIndicator {
        count: svSettingsContainer.count
        currentIndex: svSettingsContainer.currentIndex

        anchors.bottom: svSettingsContainer.bottom
        anchors.horizontalCenter: parent.horizontalCenter
    }
}
