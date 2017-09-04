/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Copyright (C) 2014 BlackBerry Limited. All rights reserved.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the QtNfc module.
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
    width: 800; height: 480
    color: "darkred"

    NearField {
        property bool requiresManualPolling: false
        orderMatch: false

        onMessageRecordsChanged: {
            var i;
            for (i = 0; i < messageRecords.length; ++i) {
                var data = "";
                if (messageRecords[i].typeNameFormat === NdefRecord.NfcRtd) {
                    if (messageRecords[i].type === "T") {
                        data = messageRecords[i].text;
                    } else if (messageRecords[i].type === "U") {
                        data = messageRecords[i].uri;
                    }
                }
                if (!data)
                    data = "Unknown content";

                list.get(listView.currentIndex).notes.append( {
                        "noteText":data
                })
            }
        }

        onPollingChanged: {
            if (!polling && requiresManualPolling)
                polling = true; //restart polling
        }

        Component.onCompleted: {
            // Polling should be true if
            // QNearFieldManager::registerNdefMessageHandler() was successful;
            // otherwise the platform requires manual polling mode.
            if (!polling) {
                requiresManualPolling = true;
                polling = true;
            }
        }
    }

    ListModel {
        id: list

        ListElement {
            name: "Personal"
            notes: [
                ListElement { noteText: "Near Field Communication" },
                ListElement { noteText: "Touch a tag and its contents will appear as a new note" }
            ]
        }

        ListElement {
            name: "Work"
            notes: [
                //ListElement { noteText: "To write a tag, click the red flag of a note and then touch a tag" },
                ListElement { noteText: "https://www.qt.io" }
            ]
        }
    }

    ListView {
        id: listView

        anchors.fill: parent
        orientation: ListView.Horizontal
        snapMode: ListView.SnapOneItem
        model: list
        highlightRangeMode: ListView.StrictlyEnforceRange
        delegate: Mode {}
    }
}
