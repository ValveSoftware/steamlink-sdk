/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtNfc module of the Qt Toolkit.
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
import QtNfc 5.2

Rectangle {
    id: root
    width: 640
    height: 360

    NearField {
        id: nearfield

        filter: [
            NdefFilter { type: "U"; typeNameFormat: NdefRecord.NfcRtd; minimum: 1; maximum: 1 },
            NdefFilter { type: "T"; typeNameFormat: NdefRecord.NfcRtd; minimum: 1 }
        ]

        onMessageRecordsChanged: {
            posterText.text = "";
            posterImage.source = "";
            posterUrl.text = "";

            var currentLocaleMatch = NdefTextRecord.LocaleMatchedNone;
            var i;
            for (i = 0; i < messageRecords.length; ++i) {
                if (messageRecords[i].recordType == "urn:nfc:wkt:T") {
                    if (messageRecords[i].localeMatch > currentLocaleMatch) {
                        currentLocaleMatch = messageRecords[i].localeMatch;
                        posterText.text = messageRecords[i].text;
                    }
                } else if (messageRecords[i].recordType == "urn:nfc:wkt:U") {
                    posterUrl.text = messageRecords[i].uri;
                } else if (messageRecords[i].recordType.substr(0, 19) == "urn:nfc:mime:image/") {
                    posterImage.source = messageRecords[i].uri;
                }
            }

            root.state = "show";
        }
    }

    Text {
        id: touchText
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        text: "Touch an NFC tag"
        font.bold: true
        font.pointSize: 18
    }

    Text {
        id: posterText
        anchors.horizontalCenter: parent.right
        anchors.horizontalCenterOffset: - parent.width / 4
        y: -height
        font.bold: true
        font.pointSize: 18
    }

    Image {
        id: posterImage
        scale: Image.PreserveAspectFit
        height:  parent.height * 0.8
        width: height * sourceSize.width / sourceSize.height
        anchors.verticalCenter: parent.verticalCenter
        x: -width
        smooth: true
    }

    Text {
        id: posterUrl
        anchors.horizontalCenter: parent.right
        anchors.horizontalCenterOffset: - parent.width / 4
        y: parent.height
        font.italic: true
        font.pointSize: 14
    }

    MouseArea {
        id: openMouseArea
        anchors.fill: parent
        enabled: root.state == "show"

        onClicked: Qt.openUrlExternally(posterUrl.text)

        Rectangle {
            id: testTouch
            width: 50
            height: 50
            color: "lightsteelblue"
            opacity: 0.3
            anchors.top:  parent.top
            anchors.right: close.left
            anchors.rightMargin: 10

            MouseArea {
                id: touchMouseArea
                anchors.fill: parent

                onClicked: {
                    if (root.state == "") {
                        root.state = "show";
                    } else {
                        root.state = "";
                    }
                }
            }
        }

        Rectangle {
            id: close
            width: 50
            height: 50
            color: "black"
            radius: 0
            opacity: 0.3
            anchors.top: parent.top
            anchors.topMargin: 0
            anchors.right: parent.right
            anchors.rightMargin: 0

            MouseArea {
                id: closeMouseArea
                anchors.fill: parent

                onClicked: Qt.quit();
            }
        }
    }

    states: [
        State {
            name: "show"
            PropertyChanges { target: posterText; y: root.height / 3 }
            PropertyChanges { target: posterUrl; y: 2 * root.height / 3 }
            PropertyChanges { target: posterImage; x: root.width / 20 }
            PropertyChanges { target: touchText; opacity: 0 }
        }
    ]

    transitions: [
        Transition {
            PropertyAnimation { easing.type: Easing.OutQuad; properties: "x,y" }
            PropertyAnimation { property: "opacity"; duration: 125 }
        }
    ]
}
