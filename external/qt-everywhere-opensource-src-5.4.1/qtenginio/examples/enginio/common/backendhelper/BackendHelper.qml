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

Rectangle {
    id: backendHelper
    anchors.fill: parent
    anchors.margins: 4
    color: "white"

    property string backendId: ""

    Behavior on opacity {
        NumberAnimation { duration: 100 }
    }

    function capitalise(string)
    {
        return string.charAt(0).toUpperCase() + string.slice(1);
    }

    Column {
        anchors.fill: parent
        spacing: 5
        Image {
            source: "qrc:images/enginio.png"
            width: parent.width
            fillMode: Image.PreserveAspectFit
        }

        Text {
            width: parent.width
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter

            font.pixelSize: 22
            font.bold: true
            text: capitalise(enginioBackendContext.exampleName) + " example"
        }

        Text {
            id: info
            width: parent.width
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 20

            text: "<html><head/><body><p>For the Enginio Examples to work the backend ID is needed. Please copy it from your <a href=\"http://engin.io\"><span style=\" text-decoration: underline; color:#0000ff;\">Enginio Dashboard</span></a>.</p><p>Make sure to have the right type of backend or configure it manually according to the example's documentation. </p></body></html>"
            color: "black"

            wrapMode: Text.Wrap
            onLinkActivated: Qt.openUrlExternally(link)
        }

        Text {
            width: parent.width
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 22
            text: capitalise(enginioBackendContext.exampleName) + " example backend id:"
        }

        TextInput {
            width: parent.width
            height: 50

            focus: true

            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 22
            text: enginioBackendContext.backendId

            onAccepted: {
                backendId = text
                enginioBackendContext.backendId = text
                backendHelper.opacity = 0
                backendHelper.enabled = false
            }

            Text {
                id: placeholderText
                anchors.fill: parent
                verticalAlignment: Text.AlignVCenter
                visible: !(parent.text.length || parent.inputMethodComposing)
                font: parent.font
                text: "backend id"
                color: "#aaa"
            }
        }
    }
}
