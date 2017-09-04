/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
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

import QtQuick 2.3
import QtNfc 5.5

Rectangle {
    id: root
    width: 640
    height: 360

    NearField {
        id: nearfield
        property bool requiresManualPolling: false

        onPollingChanged: {
            if (!polling && requiresManualPolling)
                polling = true; //restart polling
        }

        Component.onCompleted: {
            if (!polling) {
                requiresManualPolling = true;
                polling = true;
            }
        }

        //! [QML NDEF filtering]
        filter: [
            NdefFilter { type: "U"; typeNameFormat: NdefRecord.NfcRtd; maximum: 1 },
            NdefFilter { type: "T"; typeNameFormat: NdefRecord.NfcRtd },
            NdefFilter { typeNameFormat: NdefRecord.Mime; minimum: 0; maximum: 1 }
        ]

        //! [QML NDEF filtering]

        //! [messageRecordsChanged 1]
        onMessageRecordsChanged: {
        //! [messageRecordsChanged 1]
            posterText.text = "";
            posterImage.source = "";
            posterUrl.text = "";

            var currentLocaleMatch = NdefTextRecord.LocaleMatchedNone;
            var i;
            var found = false;
        //! [messageRecordsChanged 2]
            for (i = 0; i < messageRecords.length; ++i) {
                switch (messageRecords[i].typeNameFormat) {
                case NdefRecord.NfcRtd:
                    if (messageRecords[i].type === "T") {
                        if (messageRecords[i].localeMatch > currentLocaleMatch) {
                            currentLocaleMatch = messageRecords[i].localeMatch;
                            posterText.text = messageRecords[i].text;
                            found = true;
                        }

                    } else if (messageRecords[i].type === "U") {
                         posterUrl.text = messageRecords[i].uri;
                        found = true;
                    }
                    break;
                case NdefRecord.Mime:
                    if (messageRecords[i].type.indexOf("image/") === 0 ) {
                        posterImage.source = messageRecords[i].uri;
                        found = true;
                    }
                    break;
                }

                if (!found)
                    console.warn("Unknown NFC tag detected. Cannot display content.")
            }
             //! [messageRecordsChanged 2]

            root.state = "show";
        //! [messageRecordsChanged 3]
        }
        //! [messageRecordsChanged 3]
    }

    Text {
        id: touchText
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        text: "Touch an NFC tag with at least one Text and one URI record."
        font.bold: true
        font.pointSize: 18
        wrapMode: Text.WordWrap
        width: root.width*0.75
        horizontalAlignment: Text.AlignHCenter
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
        id: posterText
        anchors.horizontalCenter: parent.right
        anchors.horizontalCenterOffset: - parent.width / 4
        y: -height
        font.bold: true
        font.pointSize: 18
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
