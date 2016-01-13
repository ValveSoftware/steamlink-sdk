/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

import QtQuick 2.2
import QtQuick.Window 2.1
import QtQuick.Controls 1.2

Window {
    width: 480
    height: 640

    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: itemComponent
    }

    property StackViewDelegate fadeTransition: StackViewDelegate {
        function transitionFinished(properties)
        {
            properties.exitItem.visible = false
            properties.exitItem.opacity = 1
        }

        property Component pushTransition: StackViewTransition {
            PropertyAnimation {
                target: enterItem
                property: "opacity"
                from: 0
                to: 1
            }
            PropertyAnimation {
                target: exitItem
                property: "opacity"
                from: 1
                to: 0
            }
        }
    }

    property StackViewDelegate rotateTransition: StackViewDelegate {
        function transitionFinished(properties)
        {
            properties.exitItem.x = 0
            properties.exitItem.rotation = 0
        }

        property Component pushTransition: StackViewTransition {
            SequentialAnimation {
                ScriptAction {
                    script: enterItem.rotation = 90
                }
                PropertyAnimation {
                    target: enterItem
                    property: "x"
                    from: enterItem.width
                    to: 0
                }
                PropertyAnimation {
                    target: enterItem
                    property: "rotation"
                    from: 90
                    to: 0
                }
            }
            PropertyAnimation {
                target: exitItem
                property: "x"
                from: 0
                to: -exitItem.width
            }
        }
    }

    property StackViewDelegate slideTransition: StackViewDelegate {
        function transitionFinished(properties)
        {
            properties.exitItem.x = 0
        }

        property Component pushTransition: StackViewTransition {
            PropertyAnimation {
                target: enterItem
                property: "x"
                from: enterItem.width
                to: 0
            }
            PropertyAnimation {
                target: exitItem
                property: "x"
                from: 0
                to: exitItem.width
            }
        }
    }

    Component {
        id: itemComponent
        Item {
            id: item
            width: parent.width
            height: parent.height
            Component.onDestruction: console.log("destroyed component item: " + Stack.index)
            property bool pushFromOnCompleted: false
            Component.onCompleted: if (pushFromOnCompleted) stackView.push(itemComponent)

            Rectangle {
                anchors.fill: parent
                color: item.Stack.index % 2 ? "green" : "yellow"

                Column {
                    Text {
                        text: "This is component item: " + item.Stack.index
                    }
                    Text {
                        text: "Current status: " + item.Stack.status
                    }
                    Text { text:" " }
                    Button {
                        text: "Push component item"
                        onClicked: stackView.push(itemComponent)
                    }
                    Button {
                        text: "Push inline item"
                        onClicked: stackView.push(itemInline)
                    }
                    Button {
                        text: "Push item as JS object"
                        onClicked: stackView.push({item:itemComponent})
                    }
                    Button {
                        text: "Push immediate"
                        onClicked: stackView.push({item:itemComponent, immediate:true})
                    }
                    Button {
                        text: "Push replace"
                        onClicked: stackView.push({item:itemComponent, replace:true})
                    }
                    Button {
                        text: "Push inline item with destroyOnPop == true"
                        onClicked: stackView.push({item:itemInline, destroyOnPop:true})
                    }
                    Button {
                        text: "Push component item with destroyOnPop == false"
                        onClicked: stackView.push({item:itemComponent, destroyOnPop:false})
                    }
                    Button {
                        text: "Push from item.onCompleted"
                        onClicked: stackView.push({item:itemComponent, properties:{pushFromOnCompleted:true}})
                    }
                    Button {
                        text: "Pop"
                        onClicked: stackView.pop()
                    }
                    Button {
                        text: "Pop(null)"
                        onClicked: stackView.pop(null)
                    }
                    Button {
                        text: "Search for item 3, and pop down to it"
                        onClicked: stackView.pop(stackView.find(function(item) { if (item.Stack.index === 3) return true }))
                    }
                    Button {
                        text: "Search for item 3, and pop down to it (dontLoad == true)"
                        onClicked: stackView.pop(stackView.find(function(item) { if (item.Stack.index === 3) return true }, true))
                    }
                    Button {
                        text: "Clear"
                        onClicked: stackView.clear()
                    }
                    Button {
                        text: "Push array of 100 items"
                        onClicked: {
                            var a = new Array
                            for (var i=0; i<100; ++i)
                                a.push(itemComponent)
                            stackView.push(a)
                        }
                    }
                    Button {
                        text: "Push 10 items one by one"
                        onClicked: {
                            for (var i=0; i<10; ++i)
                                stackView.push(itemComponent)
                        }
                    }
                    Button {
                        text: "Complete transition"
                        onClicked: stackView.completeTransition()
                    }
                }
            }
        }
    }

    Item {
        id: itemInline
        visible: false
        width: parent.width
        height: parent.height
        Component.onDestruction: console.log("destroyed inline item: " + Stack.index)

        Rectangle {
            anchors.fill: parent
            color: itemInline.Stack.index % 2 ? "green" : "yellow"

            Column {
                Text {
                    text: "This is inline item: " + itemInline.Stack.index
                }
                Text {
                    text: "Current status: " + itemInline.Stack.status
                }
                Button {
                    text: "Push component item"
                    onClicked: stackView.push(itemComponent)
                }
                Button {
                    text: "Push inline item"
                    onClicked: stackView.push(itemInline)
                }
                Button {
                    text: "Push item as JS object"
                    onClicked: stackView.push({item:itemComponent})
                }
                Button {
                    text: "Push immediate"
                    onClicked: stackView.push({item:itemComponent, immediate:true})
                }
                Button {
                    text: "Push inline item with destroyOnPop == true"
                    onClicked: stackView.push({item:itemInline, destroyOnPop:true})
                }
                Button {
                    text: "Push component item with destroyOnPop == false"
                    onClicked: stackView.push({item:itemComponent, destroyOnPop:false})
                }
                Button {
                    text: "Pop"
                    onClicked: stackView.pop()
                }
                Button {
                    text: "Pop(null)"
                    onClicked: stackView.pop(null)
                }
                Button {
                    text: "Search for item 3, and pop down to it"
                    onClicked: stackView.pop(stackView.find(function(item) { if (itemInline.Stack.index === 3) return true }))
                }
                Button {
                    text: "Search for item 3, and pop down to it (dontLoad == true)"
                    onClicked: stackView.pop(stackView.find(function(item) { if (itemInline.Stack.index === 3) return true }, true))
                }
                Button {
                    text: "Clear"
                    onClicked: stackView.clear()
                }
                Button {
                    text: "Push array of 100 items"
                    onClicked: {
                        var a = new Array
                        for (var i=0; i<100; ++i)
                            a.push(itemComponent)
                        stackView.push(a)
                    }
                }
                Button {
                    text: "Push 10 items one by one"
                    onClicked: {
                        for (var i=0; i<10; ++i)
                            stackView.push(itemComponent)
                    }
                }
                Button {
                    text: "Complete transition"
                    onClicked: stackView.completeTransition()
                }
            }
        }
    }
}
