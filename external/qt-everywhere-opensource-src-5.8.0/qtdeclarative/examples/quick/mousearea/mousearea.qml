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

import QtQuick 2.4

Rectangle {
    id: box
    width: 320; height: 480

    Rectangle {
        id: redSquare
        width: 120; height: 120
        anchors.top: parent.top; anchors.left: parent.left; anchors.margins: 10
        color: "red"
        opacity: redSquareMouseArea.containsPress ? 0.6 : 1.0

        Text { text: "Click"; font.pixelSize: 16; anchors.centerIn: parent }

        MouseArea {
            id: redSquareMouseArea
            anchors.fill: parent
            hoverEnabled: true
            property string buttonID

            acceptedButtons: Qt.AllButtons
            // Value 'All.Buttons' is eqivalent to:
            // 'Qt::LeftButton | Qt::RightButton | Qt::MiddleButton  .... | Qt::ExtraButton24'

            onEntered: info.text = 'Entered'
            onExited: info.text = 'Exited (pressed=' + pressed + ')'

            onPressed: {
                if (mouse.button == Qt.LeftButton)
                    buttonID = 'LeftButton'
                else if (mouse.button == Qt.RightButton)
                    buttonID = 'RightButton'
                else if (mouse.button == Qt.MidButton)
                    buttonID = 'MiddleButton'
                else if (mouse.button == Qt.BackButton)
                    buttonID = 'BackButton'
                else if (mouse.button == Qt.ForwardButton)
                    buttonID = 'ForwardButton'
                else if (mouse.button == Qt.TaskButton)
                    buttonID = 'TaskButton'
                else if (mouse.button == Qt.ExtraButton4)
                    buttonID = 'ExtraButton4'
                else if (mouse.button == Qt.ExtraButton5)
                    buttonID = 'ExtraButton5'
                else if (mouse.button == Qt.ExtraButton6)
                    buttonID = 'ExtraButton6'
                else if (mouse.button == Qt.ExtraButton7)
                    buttonID = 'ExtraButton7'
                else if (mouse.button == Qt.ExtraButton8)
                    buttonID = 'ExtraButton8'
                else if (mouse.button == Qt.ExtraButton9)
                    buttonID = 'ExtraButton9'
                else if (mouse.button == Qt.ExtraButton10)
                    buttonID = 'ExtraButton10'
                else if (mouse.button == Qt.ExtraButton11)
                    buttonID = 'ExtraButton11'
                else if (mouse.button == Qt.ExtraButton12)
                    buttonID = 'ExtraButton12'
                else if (mouse.button == Qt.ExtraButton13)
                    buttonID = 'ExtraButton13'
                else if (mouse.button == Qt.ExtraButton14)
                    buttonID = 'ExtraButton14'
                else if (mouse.button == Qt.ExtraButton15)
                    buttonID = 'ExtraButton15'
                else if (mouse.button == Qt.ExtraButton16)
                    buttonID = 'ExtraButton16'
                else if (mouse.button == Qt.ExtraButton17)
                    buttonID = 'ExtraButton17'
                else if (mouse.button == Qt.ExtraButton18)
                    buttonID = 'ExtraButton18'
                else if (mouse.button == Qt.ExtraButton19)
                    buttonID = 'ExtraButton19'
                else if (mouse.button == Qt.ExtraButton20)
                    buttonID = 'ExtraButton20'
                else if (mouse.button == Qt.ExtraButton21)
                    buttonID = 'ExtraButton21'
                else if (mouse.button == Qt.ExtraButton22)
                    buttonID = 'ExtraButton22'
                else if (mouse.button == Qt.ExtraButton23)
                    buttonID = 'ExtraButton23'
                else if (mouse.button == Qt.ExtraButton24)
                    buttonID = 'ExtraButton24'

                info.text = 'Pressed (' + buttonID + ' shift='
                    + (mouse.modifiers & Qt.ShiftModifier ? 'true' : 'false') + ')'
                var posInBox = redSquare.mapToItem(box, mouse.x, mouse.y)
                posInfo.text = + mouse.x + ',' + mouse.y + ' in square'
                        + ' (' + posInBox.x + ',' + posInBox.y + ' in window)'
            }

            onReleased: {
                btn.text = 'Released (isClick=' + mouse.isClick + ' wasHeld=' + mouse.wasHeld + ')'
                posInfo.text = ''
            }

            //! [clicks]
            onPressAndHold: btn.text = 'Press and hold'
            onClicked: btn.text = 'Clicked (wasHeld=' + mouse.wasHeld + ')'
            onDoubleClicked: btn.text = 'Double clicked'
            //! [clicks]
        }
    }

    Rectangle {
        id: blueSquare
        width: 120; height: 120
        x: box.width - width - 10; y: 10    // making this item draggable, so don't use anchors
        color: "blue"

        Text { text: "Drag"; font.pixelSize: 16; color: "white"; anchors.centerIn: parent }

        MouseArea {
            anchors.fill: parent
            //! [drag]
            drag.target: blueSquare
            drag.axis: Drag.XAndYAxis
            drag.minimumX: 0
            drag.maximumX: box.width - parent.width
            drag.minimumY: 0
            drag.maximumY: box.height - parent.width
            //! [drag]
        }
    }

    Text {
        id: info
        anchors.bottom: btn.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.margins: 20

        onTextChanged: console.log(text)
    }

    Text {
        id: btn
        anchors.bottom: posInfo.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.margins: 20
    }

    Text {
        id: posInfo
        anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; anchors.margins: 20
    }
}
