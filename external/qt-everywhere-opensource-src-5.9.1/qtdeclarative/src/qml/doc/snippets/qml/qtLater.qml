/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the documentation of the Qt Toolkit.
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
//![0]
import QtQuick 2.0

Rectangle {
    width: 480
    height: 320

    property int callsToUpdateMinimumWidth: 0
    property bool optimize: true

    property int currentTextModel: 0
    property var columnTexts: [
        ["Click on either", "rectangle above", "and note how the counter", "below updates", "significantly faster using the", "regular (non-optimized)", "implementation"],
        ["The width", "of this column", "is", "no wider than the", "widest item"],
        ["Note how using Qt.callLater()", "the minimum width is", "calculated a bare-minimum", "number", "of times"]
    ]

    Text {
        x: 20; y: 280
        text: "Times minimum width has been calculated: " + callsToUpdateMinimumWidth
    }

    Row {
        y: 25; spacing: 30; anchors.horizontalCenter: parent.horizontalCenter
        Rectangle {
            width: 200; height:  50; color: "lightgreen"
            Text { text: "Optimized behavior\nusing Qt.callLater()"; anchors.centerIn: parent }
            MouseArea { anchors.fill: parent; onClicked: { optimize = true; currentTextModel++ } }
        }
        Rectangle {
            width: 200; height:  50; color: "lightblue"
            Text { text: "Regular behavior"; anchors.centerIn: parent}
            MouseArea { anchors.fill: parent; onClicked: { optimize = false; currentTextModel++ } }
        }
    }

    Column {
        id: column
        anchors.centerIn: parent

        onChildrenChanged: optimize ? Qt.callLater(updateMinimumWidth) : updateMinimumWidth()

        property int widestChild
        function updateMinimumWidth() {
            callsToUpdateMinimumWidth++
            var w = 0;
            for (var i in children) {
                var child = children[i];
                if (child.implicitWidth > w) {
                    w = child.implicitWidth;
                }
            }

            widestChild = w;
        }

        Repeater {
            id: repeater
            model: columnTexts[currentTextModel%3]
            delegate: Text {
                color: "white"
                text: modelData
                width: column.widestChild
                horizontalAlignment: Text.Center
                Rectangle { anchors.fill: parent; z: -1; color: index%2 ? "gray" : "darkgray" }
            }
        }
    }
}
//![0]
