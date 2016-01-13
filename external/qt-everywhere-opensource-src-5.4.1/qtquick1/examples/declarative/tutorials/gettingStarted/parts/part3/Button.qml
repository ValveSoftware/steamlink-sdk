/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

import QtQuick 1.0

Rectangle {

    //identifier of the item
    id: button

    property string label: "button label"

    //these properties act as constants, useable outside this QML file
    property int buttonHeight: 75
    property int buttonWidth: 150

    //the color highlight when the mouse hovers on the rectangle
    property color onHoverColor: "gold"
    property color borderColor: "white"

    //buttonColor is set to the button's main color
    property color buttonColor: "lightblue"

    //set appearance properties
    radius:10
    smooth: true
    border{color: "white"; width: 3}
    width: buttonWidth; height: buttonHeight

    Text{
        id: buttonLabel
        anchors.centerIn: parent
        text: label
    }

    //buttonClick() is callable and a signal handler, onButtonClick is automatically created
    signal buttonClick()
    onButtonClick: {
        console.log(buttonLabel.text + " clicked" )
    }

    //define the clickable area to be the whole rectangle
    MouseArea{
        id: buttonMouseArea
        anchors.fill: parent    //stretch the area to the parent's dimension
        onClicked: buttonClick()

        //if true, then onEntered and onExited called if mouse hovers in the mouse area
                //if false, a button must be clicked to detect the mouse hover
                hoverEnabled: true

                //display a border if the mouse hovers on the button mouse area
                onEntered: parent.border.color = onHoverColor
                //remove the border if the mouse exits the button mouse area
                onExited:  parent.border.color = borderColor

    }

    //change the color of the button when pressed
    color: buttonMouseArea.pressed ? Qt.darker(buttonColor, 1.5) : buttonColor

}
