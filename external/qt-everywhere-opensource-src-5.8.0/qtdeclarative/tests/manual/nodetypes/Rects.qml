/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
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

Item {
    Rectangle {
        width: 100
        height: 100
        anchors.centerIn: parent
        color: "red"
        NumberAnimation on rotation { from: 0; to: 360; duration: 2000; loops: Animation.Infinite; }

        Rectangle {
            color: "gray"
            width: 50
            height: 50
            anchors.centerIn: parent

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                NumberAnimation {
                    from: 1.0
                    to: 0.0
                    duration: 4000
                }
                NumberAnimation {
                    from: 0.0
                    to: 1.0
                    duration: 4000
                    easing.type: Easing.InOutQuad
                }
            }
        }
    }

    Rectangle {
        color: "green"
        width: 100
        height: 200
        x: 0
        y: 0

        NumberAnimation on x {
            from: 0
            to: 300
            duration: 5000
        }
        NumberAnimation on y {
            from: 0
            to: 50
            duration: 2000
        }

        clip: true
        Rectangle {
            color: "lightGreen"
            width: 50
            height: 50
            x: 75
            y: 175
        }
    }

    Rectangle {
        color: "blue"
        width: 200
        height: 100
        x: 100
        y: 300
        radius: 16
        border.color: "red"
        border.width: 4

        SequentialAnimation on y {
            loops: Animation.Infinite
            NumberAnimation {
                from: 300
                to: 500
                duration: 7000
            }
            NumberAnimation {
                from: 500
                to: 300
                duration: 3000
            }
        }
    }

    Rectangle {
        anchors.right: parent.right
        width: 100
        height: 100
        gradient: Gradient {
            GradientStop { position: 0.0; color: "red" }
            GradientStop { position: 0.33; color: "yellow" }
            GradientStop { position: 1.0; color: "green" }
        }
    }
}
