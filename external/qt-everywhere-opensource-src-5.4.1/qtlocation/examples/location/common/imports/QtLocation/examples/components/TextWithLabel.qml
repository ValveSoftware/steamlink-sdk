/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
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

Row {
    id: textWithLabel
    height: inputRectangle.height

    property alias label: label.text
    property alias text: inputField.text
    property alias labelWidth: label.width
    property alias font: inputField.font
    property alias busy: busyIndicator.visible

    Text {
        id: label;
        width:65;
        enabled: textWithLabel.enabled
        color: enabled ? "#242424" : "lightgrey"
        anchors.verticalCenter: parent.verticalCenter

        font.pixelSize: 14
    }

    Rectangle {
        id: inputRectangle
        width: textWithLabel.width - label.width; height: inputField.font.pixelSize * 1.5
        color: enabled ? "white" : "ghostwhite"

        radius: 5
        TextInput {
            id: inputField
            width: parent.width - anchors.leftMargin
            enabled: textWithLabel.enabled
            color: enabled ? "#242424" : "lightgrey"
            horizontalAlignment: Text.AlignLeft

            anchors {
                left: parent.left;
                verticalCenter: parent.verticalCenter;
                leftMargin: 5
            }
            font.pixelSize: 14

            BusyIndicator {
                id: busyIndicator
                height: parent.height * 0.8
                width: height
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: parent.height * 0.1
                anchors.topMargin: parent.height * 0.1
                anchors.top: parent.top
                anchors.right: parent.right
                visible: false
            }
        }
    }
}
