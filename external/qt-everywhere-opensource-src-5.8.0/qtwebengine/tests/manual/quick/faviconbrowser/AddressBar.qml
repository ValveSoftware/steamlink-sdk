/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

import QtQuick 2.5
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4

Rectangle {
    id: root

    property int progress: 0
    property url iconUrl: ""
    property url pageUrl: ""

    signal accepted(url addressUrl)

    clip: true

    onActiveFocusChanged: {
        if (activeFocus)
            addressField.forceActiveFocus();
    }

    Rectangle {
        width: addressField.width / 100 * root.progress
        height: root.height

        visible: root.progress < 100

        color: "#b6dca6"
        radius: root.radius
    }

    TextField {
        id: addressField
        anchors.fill: parent

        Image {
            anchors.verticalCenter: addressField.verticalCenter
            x: 5; z: parent.z + 1
            width: 16; height: 16
            sourceSize: Qt.size(width, height)
            source: root.iconUrl
            visible: root.progress == 100
        }

        Text {
            text: root.progress < 0 ? "" : root.progress + "%"
            x: 5; z: parent.z + 1
            font.bold: true
            anchors.verticalCenter: parent.verticalCenter

            visible: root.progress < 100
        }

        style: TextFieldStyle {
            padding.left: 30

            background: Rectangle {
                color: "transparent"
                border.color: "black"
                border.width: 1
                radius: root.radius
            }
        }

        onActiveFocusChanged: {
            if (activeFocus)
                selectAll();
            else
                deselect();
        }

        text: root.pageUrl
        onAccepted: root.accepted(utils.fromUserInput(text))
    }
}
