/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
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

import QtQuick 2.0
import QtQuick.Controls 1.1

Rectangle {
    id: page

    property real ratio: width / 320 < height / 440 ? width / 320 : height / 440
    property int elementSpacing: 6.3 * ratio

    width: 320
    height: 440

    Button {
      anchors.top: parent.top
      anchors.right: parent.right
      anchors.margins: 10
      text: hidingRect.visible ? "Hide" : "Show"
      onClicked: hidingRect.visible = !hidingRect.visible
    }

    // Create column with four rectangles, the fourth one is hidden
    Column {
        id: column

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.leftMargin: page.width / 32
        anchors.topMargin: page.height / 48
        spacing: page.elementSpacing

        //! [0]
        Rectangle {
            id: green
            color: "#80c342"
            width: 100 * page.ratio
            height: 100 * page.ratio

            Text {
              anchors.left: parent.right
              anchors.leftMargin: 20
              anchors.verticalCenter: parent.verticalCenter
              text: "Index: " + parent.Positioner.index
              + (parent.Positioner.isFirstItem ? " (First)" : "")
              + (parent.Positioner.isLastItem ? " (Last)" : "")
            }

            // When mouse is clicked, display the values of the positioner
            MouseArea {
            anchors.fill: parent
            onClicked: column.showInfo(green.Positioner)
            }
        }
        //! [0]

        Rectangle {
            id: blue
            color: "#14aaff"
            width: 100 * page.ratio
            height: 100 * page.ratio

            Text {
              anchors.left: parent.right
              anchors.leftMargin: 20
              anchors.verticalCenter: parent.verticalCenter
              text: "Index: " + parent.Positioner.index
              + (parent.Positioner.isFirstItem ? " (First)" : "")
              + (parent.Positioner.isLastItem ? " (Last)" : "")
            }

            // When mouse is clicked, display the values of the positioner
            MouseArea {
            anchors.fill: parent
            onClicked: column.showInfo(blue.Positioner)
            }
        }

        Rectangle {
            id: purple
            color: "#6400aa"
            width: 100 * page.ratio
            height: 100 * page.ratio

            Text {
              anchors.left: parent.right
              anchors.leftMargin: 20
              anchors.verticalCenter: parent.verticalCenter
              text: "Index: " + parent.Positioner.index
              + (parent.Positioner.isFirstItem ? " (First)" : "")
              + (parent.Positioner.isLastItem ? " (Last)" : "")
            }

            // When mouse is clicked, display the values of the positioner
            MouseArea {
            anchors.fill: parent
            onClicked: column.showInfo(purple.Positioner)
            }
        }

        // This rectangle is not visible, so it doesn't have a positioner value
        Rectangle {
            id: hidingRect
            color: "#006325"
            width: 100 * page.ratio
            height: 100 * page.ratio
            visible: false

            Text {
                anchors.left: parent.right
                anchors.leftMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                text: "Index: " + parent.Positioner.index
                + (parent.Positioner.isFirstItem ? " (First)" : "")
                + (parent.Positioner.isLastItem ? " (Last)" : "")
            }
        }

        // Print the index of the child item in the positioner and convenience
        // properties showing if it's the first or last item.
        function showInfo(positioner) {
            console.log("Item Index = " + positioner.index)
            console.log("  isFirstItem = " + positioner.isFirstItem)
            console.log("  isLastItem = " + positioner.isLastItem)
        }
    }
}
