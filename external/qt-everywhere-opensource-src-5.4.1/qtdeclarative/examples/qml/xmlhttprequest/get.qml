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
    width: 350; height: 400

    function showRequestInfo(text) {
        log.text = log.text + "\n" + text
        console.log(text)
    }

    Text { id: log; anchors.fill: parent; anchors.margins: 10 }

    Rectangle {
        id: button
        anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.margins: 10
        width: buttonText.width + 10; height: buttonText.height + 10
        border.width: mouseArea.pressed ? 2 : 1
        radius : 5; antialiasing: true

        Text { id: buttonText; anchors.centerIn: parent; text: "Request data.xml" }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            onClicked: {
                log.text = ""
                console.log("\n")

                var doc = new XMLHttpRequest();
                doc.onreadystatechange = function() {
                    if (doc.readyState == XMLHttpRequest.HEADERS_RECEIVED) {
                        showRequestInfo("Headers -->");
                        showRequestInfo(doc.getAllResponseHeaders ());
                        showRequestInfo("Last modified -->");
                        showRequestInfo(doc.getResponseHeader ("Last-Modified"));

                    } else if (doc.readyState == XMLHttpRequest.DONE) {
                        var a = doc.responseXML.documentElement;
                        for (var ii = 0; ii < a.childNodes.length; ++ii) {
                            showRequestInfo(a.childNodes[ii].nodeName);
                        }
                        showRequestInfo("Headers -->");
                        showRequestInfo(doc.getAllResponseHeaders ());
                        showRequestInfo("Last modified -->");
                        showRequestInfo(doc.getResponseHeader ("Last-Modified"));
                    }
                }

                doc.open("GET", "data.xml");
                doc.send();
            }
        }
    }
}

