/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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
import QtQuick.Layouts 1.0
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles.Flat 1.0 as Flat
import QtQuick.Extras 1.4
import QtQuick.Extras.Private 1.0

ApplicationWindow {
    id: window
    width: 480
    height: 860
    title: "Flat Example"
    visible: true

    readonly property bool contentLoaded: contentLoader.item
    readonly property alias anchorItem: controlsMenu
    property int currentMenu: -1
    readonly property int textMargins: Math.round(32 * Flat.FlatStyle.scaleFactor)
    readonly property int menuMargins: Math.round(13 * Flat.FlatStyle.scaleFactor)
    readonly property int menuWidth: Math.min(window.width, window.height) * 0.75

    onCurrentMenuChanged: {
        xBehavior.enabled = true;
        anchorCurrentMenu();
    }

    onMenuWidthChanged: anchorCurrentMenu()

    function anchorCurrentMenu() {
        switch (currentMenu) {
        case -1:
            anchorItem.x = -menuWidth;
            break;
        case 0:
            anchorItem.x = 0;
            break;
        case 1:
            anchorItem.x = -menuWidth * 2;
            break;
        }
    }

    Item {
        id: container
        anchors.fill: parent

        Item {
            id: loadingScreen
            anchors.fill: parent
            visible: !contentLoaded

            Timer {
                running: true
                interval: 100
                // TODO: Find a way to know when the loading screen has been rendered instead
                // of using an abritrary amount of time.
                onTriggered: contentLoader.sourceComponent = Qt.createComponent("Content.qml")
            }

            Column {
                anchors.centerIn: parent
                spacing: Math.round(10 * Flat.FlatStyle.scaleFactor)

                BusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Label {
                    text: "Loading Light Flat UI..."
                    width: Math.min(loadingScreen.width, loadingScreen.height) * 0.8
                    height: font.pixelSize
                    anchors.horizontalCenter: parent.horizontalCenter
                    renderType: Text.QtRendering
                    font.pixelSize: Math.round(26 * Flat.FlatStyle.scaleFactor)
                    horizontalAlignment: Text.AlignHCenter
                    fontSizeMode: Text.Fit
                    font.family: Flat.FlatStyle.fontFamily
                    font.weight: Font.Light
                }
            }
        }

        Rectangle {
            id: controlsMenu
            x: container.x - width
            z: contentContainer.z + 1
            width: menuWidth
            height: parent.height

            // Don't let the menus become visible when resizing the window
            Binding {
                target: controlsMenu
                property: "x"
                value: container.x - controlsMenu.width
                when: !xBehavior.enabled && !xNumberAnimation.running && currentMenu == -1
            }

            Behavior on x {
                id: xBehavior
                enabled: false
                NumberAnimation {
                    id: xNumberAnimation
                    easing.type: Easing.OutExpo
                    duration: 500
                    onRunningChanged: xBehavior.enabled = false
                }
            }

            Rectangle {
                id: controlsTitleBar
                width: parent.width
                height: toolBar.height
                color: Flat.FlatStyle.defaultTextColor

                Label {
                    text: "Controls"
                    font.family: Flat.FlatStyle.fontFamily
                    font.pixelSize: Math.round(16 * Flat.FlatStyle.scaleFactor)
                    renderType: Text.QtRendering
                    color: "white"
                    anchors.left: parent.left
                    anchors.leftMargin: menuMargins
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            ListView {
                id: controlView
                width: parent.width
                anchors.top: controlsTitleBar.bottom
                anchors.bottom: parent.bottom
                clip: true
                currentIndex: 0
                model: contentLoaded ? contentLoader.item.componentModel : null
                delegate: MouseArea {
                    id: delegateItem
                    width: parent.width
                    height: 64 * Flat.FlatStyle.scaleFactor
                    onClicked: {
                        if (controlView.currentIndex != index)
                            controlView.currentIndex = index;

                        currentMenu = -1;
                    }

                    Rectangle {
                        width: parent.width
                        height: Flat.FlatStyle.onePixel
                        anchors.bottom: parent.bottom
                        color: Flat.FlatStyle.lightFrameColor
                    }

                    Label {
                        text: delegateItem.ListView.view.model[index].name
                        font.pixelSize: Math.round(15 * Flat.FlatStyle.scaleFactor)
                        font.family: Flat.FlatStyle.fontFamily
                        renderType: Text.QtRendering
                        color: delegateItem.ListView.isCurrentItem ? Flat.FlatStyle.styleColor : Flat.FlatStyle.defaultTextColor
                        anchors.left: parent.left
                        anchors.leftMargin: menuMargins
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Rectangle {
                    width: parent.height
                    height: 8 * Flat.FlatStyle.scaleFactor
                    rotation: 90
                    anchors.left: parent.right
                    transformOrigin: Item.TopLeft

                    gradient: Gradient {
                        GradientStop {
                            color: Qt.rgba(0, 0, 0, 0.15)
                            position: 0
                        }
                        GradientStop {
                            color: Qt.rgba(0, 0, 0, 0.05)
                            position: 0.5
                        }
                        GradientStop {
                            color: Qt.rgba(0, 0, 0, 0)
                            position: 1
                        }
                    }
                }
            }
        }

        Item {
            id: contentContainer
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: controlsMenu.right
            width: parent.width

            ToolBar {
                id: toolBar
                visible: !loadingScreen.visible
                width: parent.width
                height: 54 * Flat.FlatStyle.scaleFactor
                z: contentLoader.z + 1
                style: Flat.ToolBarStyle {
                    padding.left: 0
                    padding.right: 0
                }

                RowLayout {
                    anchors.fill: parent

                    MouseArea {
                        id: controlsButton
                        Layout.preferredWidth: toolBar.height + textMargins
                        Layout.preferredHeight: toolBar.height
                        onClicked: currentMenu = currentMenu == 0 ? -1 : 0

                        Column {
                            id: controlsIcon
                            anchors.left: parent.left
                            anchors.leftMargin: textMargins
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Math.round(2 * Flat.FlatStyle.scaleFactor)

                            Repeater {
                                model: 3

                                Rectangle {
                                    width: Math.round(4 * Flat.FlatStyle.scaleFactor)
                                    height: width
                                    radius: width / 2
                                    color: Flat.FlatStyle.defaultTextColor
                                }
                            }
                        }
                    }

                    Text {
                        text: "Light Flat UI Demo"
                        font.family: Flat.FlatStyle.fontFamily
                        font.pixelSize: Math.round(16 * Flat.FlatStyle.scaleFactor)
                        horizontalAlignment: Text.AlignHCenter
                        color: "#666666"
                        Layout.fillWidth: true
                    }

                    MouseArea {
                        id: settingsButton
                        Layout.preferredWidth: controlsButton.Layout.preferredWidth
                        Layout.preferredHeight: controlsButton.Layout.preferredHeight
                        onClicked: currentMenu = currentMenu == 1 ? -1 : 1

                        SettingsIcon {
                            width: Math.round(32 * Flat.FlatStyle.scaleFactor)
                            height: Math.round(32 * Flat.FlatStyle.scaleFactor)
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            anchors.rightMargin: textMargins - Math.round(8 * Flat.FlatStyle.scaleFactor)
                        }
                    }
                }
            }

            Loader {
                id: contentLoader
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: toolBar.bottom
                anchors.bottom: parent.bottom

                property QtObject settingsData: QtObject {
                    readonly property bool checks: disableSingleItemsAction.checked
                    readonly property bool frames: !greyBackgroundAction.checked
                    readonly property bool allDisabled: disableAllAction.checked
                }
                property QtObject controlData: QtObject {
                    readonly property int componentIndex: controlView.currentIndex
                    readonly property int textMargins: window.textMargins
                }

                MouseArea {
                    enabled: currentMenu != -1
                    // We would be able to just set this to true here, if it weren't for QTBUG-43083.
                    hoverEnabled: enabled
                    anchors.fill: parent
                    preventStealing: true
                    onClicked: currentMenu = -1
                    focus: enabled
                    z: 1000
                }
            }
        }

        Rectangle {
            id: settingsMenu
            z: contentContainer.z + 1
            width: menuWidth
            height: parent.height
            anchors.left: contentContainer.right

            Rectangle {
                id: optionsTitleBar
                width: parent.width
                height: toolBar.height
                color: Flat.FlatStyle.defaultTextColor

                Label {
                    text: "Options"
                    font.family: Flat.FlatStyle.fontFamily
                    font.pixelSize: Math.round(16 * Flat.FlatStyle.scaleFactor)
                    renderType: Text.QtRendering
                    color: "white"
                    anchors.left: parent.left
                    anchors.leftMargin: menuMargins
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Action {
                id: disableAllAction
                checkable: true
                checked: false
            }

            Action {
                id: disableSingleItemsAction
                checkable: true
                checked: false
            }

            Action {
                id: greyBackgroundAction
                checkable: true
                checked: false
            }

            ListView {
                id: optionsListView
                width: parent.width
                anchors.top: optionsTitleBar.bottom
                anchors.bottom: parent.bottom
                clip: true
                interactive: delegateHeight * count > height

                readonly property int delegateHeight: 64 * Flat.FlatStyle.scaleFactor

                model: [
                    { name: "Disable all", action: disableAllAction },
                    { name: "Disable single items", action: disableSingleItemsAction },
                    { name: "Grey background", action: greyBackgroundAction },
                    { name: "Exit", action: null }
                ]
                delegate: Rectangle {
                    id: optionDelegateItem
                    width: parent.width
                    height: optionsListView.delegateHeight

                    Rectangle {
                        width: parent.width
                        height: Flat.FlatStyle.onePixel
                        anchors.bottom: parent.bottom
                        color: Flat.FlatStyle.lightFrameColor
                    }

                    Loader {
                        sourceComponent: optionText !== "Exit"
                            ? optionsListView.checkBoxComponent : optionsListView.exitComponent
                        anchors.fill: parent
                        anchors.leftMargin: menuMargins

                        property string optionText: optionsListView.model[index].name
                        property int optionIndex: index
                    }
                }

                property Component checkBoxComponent: Item {
                    Label {
                        text: optionText
                        font.family: Flat.FlatStyle.fontFamily
                        font.pixelSize: Math.round(15 * Flat.FlatStyle.scaleFactor)
                        fontSizeMode: Text.Fit
                        renderType: Text.QtRendering
                        verticalAlignment: Text.AlignVCenter
                        color: Flat.FlatStyle.defaultTextColor
                        height: parent.height
                        anchors.left: parent.left
                        anchors.right: checkBox.left
                        anchors.rightMargin: Flat.FlatStyle.twoPixels
                    }

                    CheckBox {
                        id: checkBox
                        checked: optionsListView.model[optionIndex].action.checked
                        anchors.right: parent.right
                        anchors.rightMargin: menuMargins
                        anchors.verticalCenter: parent.verticalCenter
                        onCheckedChanged: optionsListView.model[optionIndex].action.checked = checkBox.checked
                    }
                }

                property Component exitComponent: MouseArea {
                    anchors.fill: parent
                    onClicked: Qt.quit()

                    Label {
                        text: optionText
                        font.family: Flat.FlatStyle.fontFamily
                        font.pixelSize: Math.round(15 * Flat.FlatStyle.scaleFactor)
                        renderType: Text.QtRendering
                        color: Flat.FlatStyle.defaultTextColor
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Rectangle {
                    width: parent.height
                    height: 8 * Flat.FlatStyle.scaleFactor
                    rotation: -90
                    anchors.right: parent.left
                    transformOrigin: Item.TopRight

                    gradient: Gradient {
                        GradientStop {
                            color: Qt.rgba(0, 0, 0, 0.15)
                            position: 0
                        }
                        GradientStop {
                            color: Qt.rgba(0, 0, 0, 0.05)
                            position: 0.5
                        }
                        GradientStop {
                            color: Qt.rgba(0, 0, 0, 0)
                            position: 1
                        }
                    }
                }
            }
        }
    }
}
