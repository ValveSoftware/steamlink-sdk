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

Component {
    Item {
        property variant stickies

        id: page
        width: ListView.view.width+40; height: ListView.view.height


        Image {
            source: "cork.jpg"
            width: page.ListView.view.width
            height: page.ListView.view.height
            fillMode: Image.PreserveAspectCrop
            clip: true
        }

        MouseArea {
            anchors.fill: parent
            onClicked: page.focus = false;
        }

        Text {
            text: name; x: 15; y: 8; height: 40; width: 370
            font.pixelSize: 18; font.bold: true; color: "white"
            style: Text.Outline; styleColor: "black"
        }

        Repeater {
            model: notes
            Item {
                id: stickyPage

                property int randomX: Math.random() * (page.ListView.view.width-0.5*stickyImage.width) +100
                property int randomY: Math.random() * (page.ListView.view.height-0.5*stickyImage.height) +50

                x: randomX; y: randomY

                rotation: -flickable.horizontalVelocity / 100;
                Behavior on rotation {
                    SpringAnimation { spring: 2.0; damping: 0.15 }
                }

                Item {
                    id: sticky
                    scale: 0.7

                    Image {
                        id: stickyImage
                        x: 8 + -width * 0.6 / 2; y: -20
                        source: "note-yellow.png"
                        scale: 0.6; transformOrigin: Item.TopLeft
                    }

                    TextEdit {
                        id: myText
                        x: -104; y: 36; width: 215; height: 200
                        font.pixelSize: 24
                        readOnly: false
                        rotation: -8
                        text: noteText
                    }

                    Item {
                        x: stickyImage.x; y: -20
                        width: stickyImage.width * stickyImage.scale
                        height: stickyImage.height * stickyImage.scale

                        MouseArea {
                            id: mouse
                            anchors.fill: parent
                            drag.target: stickyPage
                            drag.axis: Drag.XAndYAxis
                            drag.minimumY: 0
                            drag.maximumY: page.height - 80
                            drag.minimumX: 100
                            drag.maximumX: page.width - 140
                            onClicked: myText.forceActiveFocus()
                        }
                    }
                }

                Image {
                    x: -width / 2; y: -height * 0.5 / 2
                    source: "tack.png"
                    scale: 0.7; transformOrigin: Item.TopLeft
                }

                states: State {
                    name: "pressed"
                    when: mouse.pressed
                    PropertyChanges { target: sticky; rotation: 8; scale: 1 }
                    PropertyChanges { target: page; z: 8 }
                }

                transitions: Transition {
                    NumberAnimation { properties: "rotation,scale"; duration: 200 }
                }
            }
        }
    }
}








