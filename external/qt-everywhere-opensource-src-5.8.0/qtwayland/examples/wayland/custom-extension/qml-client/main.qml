/****************************************************************************
**
** Copyright (C) 2016 Erik Larsson.
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
import QtQuick.Window 2.0
import com.theqtcompany.customextension 1.0

Window {
    id: topLevelWindow
    visible: true
    Rectangle {
        anchors.fill: parent
        color: "#f1eece"
    }
    property alias textItem: bounceText
    Text {
        id: bounceText
        text: "press here to bounce"
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            if (customExtension.active)
                customExtension.sendBounce(topLevelWindow, 1000)
        }
    }

    MouseArea {
        anchors.centerIn: parent
        width: 100; height: 100
        onClicked: {
            if (customExtension.active)
                customExtension.sendSpin(topLevelWindow, 500)
        }

        Rectangle {
            anchors.fill: parent
            color: "#fab1ed"
            Text {
                text: "spin"
            }
        }
    }

    CustomExtension {
        id: customExtension
        onActiveChanged: {
            console.log("Custom extension is active:", active)
            registerWindow(topLevelWindow)
        }
        onFontSize: {
            // signal arguments: window and pixelSize
            // we are free to interpret the protocol as we want, so
            // let's change the font size of just one of the text items
            window.textItem.font.pixelSize = pixelSize
        }
    }
}

