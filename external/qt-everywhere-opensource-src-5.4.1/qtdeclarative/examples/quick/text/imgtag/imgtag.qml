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
    id: main
    width: 320; height: 480
    focus: true
    color: "#dedede"

    property var hAlign: Text.AlignLeft

    Flickable {
        anchors.fill: parent
        contentWidth: parent.width
        contentHeight: col.height + 20

        Column {
            id: col
            x: 10; y: 10
            spacing: 20
            width: parent.width - 20

            TextWithImage {
                text: "This is a <b>happy</b> face<img src=\"images/face-smile.png\">"
            }
            TextWithImage {
                text: "This is a <b>very<img src=\"images/face-smile-big.png\" align=\"middle\"/>happy</b> face vertically aligned in the middle."
            }
            TextWithImage {
                text: "This is a tiny<img src=\"images/face-smile.png\" width=\"15\" height=\"15\">happy face."
            }
            TextWithImage {
                text: "This is a<img src=\"images/starfish_2.png\" width=\"50\" height=\"50\" align=\"top\">aligned to the top and a<img src=\"images/heart200.png\" width=\"50\" height=\"50\">aligned to the bottom."
            }
            TextWithImage {
                text: "Qt logos<img src=\"images/qtlogo.png\" width=\"55\" height=\"60\" align=\"middle\"><img src=\"images/qtlogo.png\" width=\"37\" height=\"40\" align=\"middle\"><img src=\"images/qtlogo.png\" width=\"18\" height=\"20\" align=\"middle\">aligned in the middle with different sizes."
            }
            TextWithImage {
                text: "Some hearts<img src=\"images/heart200.png\" width=\"20\" height=\"20\" align=\"bottom\"><img src=\"images/heart200.png\" width=\"30\" height=\"30\" align=\"bottom\"> <img src=\"images/heart200.png\" width=\"40\" height=\"40\"><img src=\"images/heart200.png\" width=\"50\" height=\"50\" align=\"bottom\">with different sizes."
            }
            TextWithImage {
                text: "Resized image<img width=\"48\" height=\"48\" align=\"middle\" src=\"http://qt-project.org/images/qt13a/Qt-logo.png\">from the internet."
            }
            TextWithImage {
                text: "Image<img align=\"middle\" src=\"http://qt-project.org/images/qt13a/Qt-logo.png\">from the internet."
            }
            TextWithImage {
                height: 120
                verticalAlignment: Text.AlignVCenter
                text: "This is a <b>happy</b> face<img src=\"images/face-smile.png\"> with an explicit height."
            }
        }
    }

    Keys.onUpPressed: main.hAlign = Text.AlignHCenter
    Keys.onLeftPressed: main.hAlign = Text.AlignLeft
    Keys.onRightPressed: main.hAlign = Text.AlignRight
}
