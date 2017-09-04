/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
import QtQuick 2.1
import QtWinExtras 1.0
import QtQuick.Layouts 1.0
import QtQuick.Controls 1.0

ApplicationWindow {
    id: window

    title: "ThumbnailToolBar"

    width: 800
    height: 400
    minimumWidth: 480
    minimumHeight: 320

    ThumbnailToolBar {
        id: toolBar
        iconicThumbnailSource : "qrc:/qt-project.org/qmessagebox/images/qtlogo-64.png"
        iconicLivePreviewSource : "qrc:/qt-project.org/qmessagebox/images/qtlogo-64.png"

        ThumbnailToolButton {
            tooltip: "Button #1"
            iconSource: "qrc:/qt-project.org/qmessagebox/images/qtlogo-64.png"
        }

        Component {
            id: buttonComponent
            ThumbnailToolButton {
                iconSource: "qrc:/qt-project.org/qmessagebox/images/qtlogo-64.png"
            }
        }
    }

    RowLayout {
        id: row

        spacing: 10
        anchors.margins: 10
        anchors.fill: parent

        Item {
            width: window.width / 3 * 2
            Layout.fillWidth: true
            Layout.fillHeight: true

            GroupBox {
                anchors.fill: parent
                title: qsTr("Buttons (%1)").arg(toolBar.count)

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10

                    TableView {
                        id: tableView

                        currentRow: 0
                        model: toolBar.buttons

                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        TableViewColumn { role: "tooltip"; title: "Tooltip"; width: 132 }
                        TableViewColumn { role: "iconSource"; title: "Icon"; width: 88 }
                        TableViewColumn { role: "visible"; title: "Visible"; width: 64 }
                        TableViewColumn { role: "enabled"; title: "Enabled"; width: 64 }
                        TableViewColumn { role: "interactive"; title: "Interactive"; width: 64 }
                        TableViewColumn { role: "flat"; title: "Flat"; width: 64 }
                        TableViewColumn { role: "dismissOnClick"; title: "Dismissable"; width: 64 }
                    }

                    RowLayout {
                        spacing: 10
                        anchors.horizontalCenter: parent.horizontalCenter

                        ToolButton {
                            property int counter: 1

                            text: "Add"
                            onClicked: {
                                var btn = buttonComponent.createObject(toolBar)
                                btn.tooltip = qsTr("Button #%1").arg(++counter)
                                toolBar.addButton(btn)
                                tableView.currentRow = toolBar.count - 1
                            }
                        }

                        ToolButton {
                            text: "Remove"
                            enabled: toolBar.count > 0 && tableView.currentRow !== -1
                            onClicked: {
                                toolBar.removeButton(toolBar.buttons[tableView.currentRow])
                            }
                        }

                        ToolButton {
                            text: "Clear"
                            enabled: toolBar.count > 0
                            onClicked: {
                                toolBar.clear()
                            }
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillHeight: true
            width: repeater.count ? repeater.itemAt(0).implicitWidth : 0

            Repeater {
                id: repeater

                model: toolBar.buttons

                GroupBox {
                    title: "Properties"

                    anchors.fill: parent
                    visible: index === tableView.currentRow
                    implicitWidth: grid.implicitWidth + 20

                    GridLayout {
                        id: grid

                        columns: 2
                        rowSpacing: 10
                        columnSpacing: 10
                        anchors.fill: parent
                        anchors.margins: 10

                        property ThumbnailToolButton button: toolBar.buttons[index]

                        Label { text: "Tooltip:" }
                        TextField {
                            text: grid.button ? grid.button.tooltip : ""
                            onTextChanged: if (grid.button) grid.button.tooltip = text
                            Layout.fillWidth: true
                        }

                        Label { text: "Icon:" }
                        ComboBox {
                            currentIndex: 1
                            textRole: "name"
                            Layout.fillWidth: true
                            model: ListModel {
                                id: iconModel
                                ListElement { name: "None"; source: "" }
                                ListElement { name: "Qt logo (64)"; source: "qrc:/qt-project.org/qmessagebox/images/qtlogo-64.png" }
                                ListElement { name: "Up (16)"; source: "qrc:/qt-project.org/styles/commonstyle/images/up-16.png" }
                                ListElement { name: "Left (32)"; source: "qrc:/qt-project.org/styles/commonstyle/images/left-32.png" }
                                ListElement { name: "Right (32)"; source: "qrc:/qt-project.org/styles/commonstyle/images/right-32.png" }
                                ListElement { name: "Down (128)"; source: "qrc:/qt-project.org/styles/commonstyle/images/down-128.png" }
                                ListElement { name: "Non-existing"; source: "something that does not exist" }
                            }
                            onCurrentIndexChanged: {
                                if (grid.button && currentIndex !== -1)
                                    grid.button.iconSource = iconModel.get(currentIndex).source
                            }
                        }

                        CheckBox {
                            text: "Visible"
                            checked: grid.button && grid.button.visible
                            onCheckedChanged: if (grid.button) grid.button.visible = checked
                            Layout.columnSpan: 2
                        }

                        CheckBox {
                            text: "Enabled"
                            checked: grid.button && grid.button.enabled
                            onCheckedChanged: if (grid.button) grid.button.enabled = checked
                            Layout.columnSpan: 2
                        }

                        CheckBox {
                            text: "Interactive"
                            checked: grid.button && grid.button.interactive
                            onCheckedChanged: if (grid.button) grid.button.interactive = checked
                            Layout.columnSpan: 2
                        }

                        CheckBox {
                            text: "Flat"
                            checked: grid.button && grid.button.flat
                            onCheckedChanged: if (grid.button) grid.button.flat = checked
                            Layout.columnSpan: 2
                        }

                        CheckBox {
                            text: "Dismissable"
                            checked: grid.button && grid.button.dismissOnClick
                            onCheckedChanged: if (grid.button) grid.button.dismissOnClick = checked
                            Layout.columnSpan: 2
                        }

                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }
    }
}
