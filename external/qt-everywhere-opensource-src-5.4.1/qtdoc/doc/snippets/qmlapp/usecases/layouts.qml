/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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

//![import]
import QtQuick 2.3

//![import]

Column {

//![direct]
Item {
    width: 100; height: 100

    Rectangle {
        // Manually positioned at 20,20
        x: 20
        y: 20
        width: 80
        height: 80
        color: "red"
    }
}
//![direct]

//![anchors]
Item {
    width: 200; height: 200

    Rectangle {
        // Anchored to 20px off the top right corner of the parent
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 20 // Sets all margins at once

        width: 80
        height: 80
        color: "orange"
    }

    Rectangle {
        // Anchored to 20px off the top center corner of the parent.
        // Notice the different group property syntax for 'anchors' compared to
        // the previous Rectangle. Both are valid.
        anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 20 }

        width: 80
        height: 80
        color: "green"
    }
}
//![anchors]

//![positioners]
Item {
    width: 300; height: 100

    Row { // The "Row" type lays out its child items in a horizontal line
        spacing: 20 // Places 20px of space between items

        Rectangle { width: 80; height: 80; color: "red" }
        Rectangle { width: 80; height: 80; color: "green" }
        Rectangle { width: 80; height: 80; color: "blue" }
    }
}
//![positioners]

Item {
    width: 300; height: 400

    Rectangle {
        id: middleRect
        //This Rectangle has its y animated, for the others to follow
        x: 120
        y: 220
        SequentialAnimation on y {
            loops: -1
            NumberAnimation { from: 220; to: 380; easing.type: Easing.InOutQuad; duration: 1200 }
            NumberAnimation { from: 380; to: 220; easing.type: Easing.InOutQuad; duration: 1200 }
        }
        width: 80
        height: 80
        color: "green"
    }
    Rectangle {
        // x,y bound to the position of middleRect
        x: middleRect.x - 100
        y: middleRect.y
        width: 80
        height: 80
        color: "red"
    }

    Rectangle {
        // Anchored to the position of middleRect
        anchors.left: middleRect.right
        anchors.leftMargin: 20
        anchors.verticalCenter: middleRect.verticalCenter
        width: 80
        height: 80
        color: "blue"
    }
}

}
