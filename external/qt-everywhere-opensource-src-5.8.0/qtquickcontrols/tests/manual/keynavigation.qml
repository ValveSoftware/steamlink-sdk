/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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
import QtQuick.Controls 1.2

ApplicationWindow {
    width: 400
    height: 400

    Rectangle {
        id: back
        anchors.fill: parent
        color: enabled ? "lightgray" : "wheat"
    }

    Column {
        spacing: 8
        anchors.centerIn: parent

        Row {
            Button {
                id: button1
                focus: true
                text: "Button 1"
                activeFocusOnPress: true
                KeyNavigation.tab: button2
                onClicked: back.enabled = !back.enabled
            }
            Button {
                id: button2
                text: "Button 2"
                activeFocusOnPress: true
                KeyNavigation.tab: button3
                KeyNavigation.backtab: button1
                onClicked: back.enabled = !back.enabled
            }
            Button {
                id: button3
                text: "Button 3"
                activeFocusOnPress: true
                KeyNavigation.tab: checkbox1
                KeyNavigation.backtab: button2
                onClicked: back.enabled = !back.enabled
            }
        }

        Row {
            CheckBox {
                id: checkbox1
                text: "Checkbox 1"
                activeFocusOnPress: true
                KeyNavigation.tab: checkbox2
                KeyNavigation.backtab: button3
                onClicked: back.enabled = !back.enabled
            }
            CheckBox {
                id: checkbox2
                text: "Checkbox 2"
                activeFocusOnPress: true
                KeyNavigation.tab: checkbox3
                KeyNavigation.backtab: checkbox1
                onClicked: back.enabled = !back.enabled
            }
            CheckBox {
                id: checkbox3
                text: "Checkbox 3"
                activeFocusOnPress: true
                KeyNavigation.tab: radioButton1
                KeyNavigation.backtab: checkbox2
                onClicked: back.enabled = !back.enabled
            }
        }

        Row {
            ExclusiveGroup { id: exclusive }
            RadioButton {
                id: radioButton1
                text: "RadioButton 1"
                exclusiveGroup: exclusive
                activeFocusOnPress: true
                KeyNavigation.tab: radioButton2
                KeyNavigation.backtab: checkbox3
                onClicked: back.enabled = !back.enabled
            }
            RadioButton {
                id: radioButton2
                text: "RadioButton 2"
                exclusiveGroup: exclusive
                activeFocusOnPress: true
                KeyNavigation.tab: radioButton3
                KeyNavigation.backtab: radioButton1
                onClicked: back.enabled = !back.enabled
            }
            RadioButton {
                id: radioButton3
                text: "RadioButton 3"
                exclusiveGroup: exclusive
                activeFocusOnPress: true
                KeyNavigation.backtab: radioButton2
                onClicked: back.enabled = !back.enabled
            }
        }
    }
}
