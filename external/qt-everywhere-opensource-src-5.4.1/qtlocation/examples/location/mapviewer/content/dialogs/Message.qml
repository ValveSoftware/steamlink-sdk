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
import QtLocation.examples 5.0

Item {
    id: dialog

    anchors.fill: parent

    property alias title: titleBar.text
    property alias text: message.text
    property int gap: 10

    signal okButtonClicked
    signal cancelButtonClicked
    opacity: 0
    enabled: opacity > 0 ? true : false

    Fader {}

    Rectangle {
        id: dialogRectangle

        color: "#ECECEC"
        width: parent.width - gap;
        height: titleBar.height + message.height + okButton.height + gap*3
        anchors {
            verticalCenter: parent.verticalCenter
            left: parent.left
            leftMargin: gap/2
        }

        radius: 5

        TitleBar {
            id: titleBar;
            width: parent.width; height: 40;
            anchors.top: parent.top; anchors.left: parent.left;
            opacity: 0.9;
            onClicked: { dialog.cancelButtonClicked() }
        }

        Text {
            id: message
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
            anchors.top: titleBar.bottom
            width: dialogRectangle.width - gap
            anchors.topMargin: gap
            textFormat: Text.RichText
            wrapMode: Text.Wrap
            onLinkActivated: {
                Qt.openUrlExternally(link)
            }
            font.pixelSize: 14
        }

        Button {
            id: okButton
            text: "Ok"
            anchors.top: message.bottom
            anchors.topMargin: gap
            width: 80; height: 32
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: {
                dialog.okButtonClicked ()
            }
        }
    }
}
