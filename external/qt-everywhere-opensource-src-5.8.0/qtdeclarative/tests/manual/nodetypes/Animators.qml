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

import QtQuick 2.5

Item {
    id: window

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#14148c" }
            GradientStop { position: 0.499; color: "#14aaff" }
            GradientStop { position: 0.5; color: "#80c342" }
            GradientStop { position: 1.0; color: "#006325" }
        }
    }

    SequentialAnimation {
        id: plainAnim
        SequentialAnimation {
            ParallelAnimation {
                PropertyAnimation {
                    property: "y"
                    target: smiley
                    from: smiley.minHeight
                    to: smiley.maxHeight
                    easing.type: Easing.OutExpo
                    duration: 300
                }
                PropertyAnimation {
                    property: "scale"
                    target: shadow
                    from: 1
                    to: 0.5
                    easing.type: Easing.OutExpo
                    duration: 300
                }
            }
            ParallelAnimation {
                PropertyAnimation {
                    property: "y"
                    target: smiley
                    from: smiley.maxHeight
                    to: smiley.minHeight
                    easing.type: Easing.OutBounce
                    duration: 1000
                }
                PropertyAnimation {
                    property: "scale"
                    target: shadow
                    from: 0.5
                    to: 1
                    easing.type: Easing.OutBounce
                    duration: 1000
                }
            }
        }
        running: false
    }

    SequentialAnimation {
        id: renderThreadAnim
        SequentialAnimation {
            ParallelAnimation {
                YAnimator {
                    target: smiley
                    from: smiley.minHeight
                    to: smiley.maxHeight
                    easing.type: Easing.OutExpo
                    duration: 300
                }
                ScaleAnimator {
                    target: shadow
                    from: 1
                    to: 0.5
                    easing.type: Easing.OutExpo
                    duration: 300
                }
            }
            ParallelAnimation {
                YAnimator {
                    target: smiley
                    from: smiley.maxHeight
                    to: smiley.minHeight
                    easing.type: Easing.OutBounce
                    duration: 1000
                }
                ScaleAnimator {
                    target: shadow
                    from: 0.5
                    to: 1
                    easing.type: Easing.OutBounce
                    duration: 1000
                }
            }
        }
        running: false
    }

    Image {
        id: shadow
        anchors.horizontalCenter: parent.horizontalCenter
        y: smiley.minHeight + smiley.height
        source: "qrc:/shadow.png"
    }

    Image {
        id: smiley
        property int maxHeight: window.height / 3
        property int minHeight: 2 * window.height / 3

        anchors.horizontalCenter: parent.horizontalCenter
        y: minHeight
        source: "qrc:/face-smile.png"
    }

    Text {
        text: "click left for plain animation, right for render thread Animators, middle to sleep for 2 sec on the main (gui) thread"
        color: "white"
    }

    Text {
        text: plainAnim.running ? "NORMAL ANIMATION" : (renderThreadAnim.running ? "RENDER THREAD ANIMATION" : "NO ANIMATION")
        color: "red"
        font.pointSize: 20
        anchors.bottom: parent.bottom
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onClicked: if (mouse.button === Qt.LeftButton) {
                       renderThreadAnim.running = false;
                       plainAnim.running = true;
                   } else if (mouse.button === Qt.RightButton) {
                       plainAnim.running = false;
                       renderThreadAnim.running = true;
                   } else if (mouse.button === Qt.MiddleButton) {
                       helper.sleep(2000);
                   }
    }
}
