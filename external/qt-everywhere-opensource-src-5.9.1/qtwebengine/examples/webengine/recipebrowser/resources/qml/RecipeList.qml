/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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
import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.0
import QtQuick.Layouts 1.0

FocusScope {
    id: root
    signal recipeSelected(url url)

    ColumnLayout {
        spacing: 0
        anchors.fill: parent

        ToolBar {
            id: headerBackground
            Layout.fillWidth: true
            implicitHeight: headerText.height + 20

            Label {
                id: headerText
                width: parent.width
                text: qsTr("Favorite recipes")
                padding: 10
                anchors.centerIn: parent
            }
        }

        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            keyNavigationWraps: true
            clip: true
            focus: true
            ScrollBar.vertical: ScrollBar { }

            model: recipeModel

            delegate: ItemDelegate {
                width: parent.width
                text: model.name
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: parent.enabled ? parent.Material.primaryTextColor
                                          : parent.Material.hintTextColor
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.Wrap
                }

                property url url: model.url
                highlighted: ListView.isCurrentItem

                onClicked: {
                    listView.forceActiveFocus()
                    listView.currentIndex = model.index
                }
            }

            onCurrentItemChanged: {
                root.recipeSelected(currentItem.url)
            }

            ListModel {
                id: recipeModel

                ListElement {
                    name: "Pizza Diavola"
                    url: "qrc:///pages/pizza.html"
                }
                ListElement {
                    name: "Steak"
                    url: "qrc:///pages/steak.html"
                }
                ListElement {
                    name: "Burger"
                    url: "qrc:///pages/burger.html"
                }
                ListElement {
                    name: "Soup"
                    url: "qrc:///pages/soup.html"
                }
                ListElement {
                    name: "Pasta"
                    url: "qrc:///pages/pasta.html"
                }
                ListElement {
                    name: "Grilled Skewers"
                    url: "qrc:///pages/skewers.html"
                }
                ListElement {
                    name: "Cupcakes"
                    url: "qrc:///pages/cupcakes.html"
                }
            }

            ToolTip {
                id: help
                implicitWidth: root.width - padding * 3
                y: root.y + root.height
                delay: 1000
                timeout: 5000
                text: qsTr("Use keyboard, mouse, or touch controls to navigate through the\
                            recipes.")

                contentItem: Text {
                    text: help.text
                    font: help.font
                    color: help.Material.primaryTextColor
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    function showHelp() {
        help.open()
    }
}

