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

    title: "TaskbarButton"

    width: column.implicitWidth + 240
    height: column.implicitHeight + 160
    minimumWidth: column.implicitWidth + 40
    minimumHeight: column.implicitHeight + 40

    TaskbarButton {
        id: taskbar

        progress.visible: progressBox.checked
        progress.minimum: indeterminateBox.checked ? 0 : minSpinBox.value
        progress.maximum: indeterminateBox.checked ? 0 : maxSpinBox.value
        progress.value: indeterminateBox.checked ? 0 : valueSlider.value

        overlay.iconSource: overlayBox.checked && overlayCombo.currentIndex >= 0 ? overlayModel.get(overlayCombo.currentIndex).source : ""
        overlay.accessibleDescription: "TaskbarButton"
    }

    ColumnLayout {
        id: column

        spacing: 20
        anchors.centerIn: parent
        Layout.preferredWidth: window.width / 2

        GroupBox {
            id: overlayBox

            checked: false
            checkable: true
            title: "Overlay icon"
            Layout.fillWidth: true

            ComboBox {
                id: overlayCombo

                anchors.fill: parent
                anchors.margins: 10

                textRole: "name"
                model: ListModel {
                    id: overlayModel
                    ListElement { name: "Qt logo (64)"; source: "qrc:/qt-project.org/qmessagebox/images/qtlogo-64.png" }
                    ListElement { name: "Up (16)"; source: "qrc:/qt-project.org/styles/commonstyle/images/up-16.png" }
                    ListElement { name: "Left (32)"; source: "qrc:/qt-project.org/styles/commonstyle/images/left-32.png" }
                    ListElement { name: "Right (32)"; source: "qrc:/qt-project.org/styles/commonstyle/images/right-32.png" }
                    ListElement { name: "Down (128)"; source: "qrc:/qt-project.org/styles/commonstyle/images/down-128.png" }
                    ListElement { name: "Remote"; source: "http://qt-project.org/images/qt13a/Qt-logo.png" }
                    ListElement { name: "Non-existing"; source: "do not crash explorer" }
                }
            }
        }

        GroupBox {
            id: progressBox

            checked: false
            checkable: true
            title: qsTr("Progress (%1)").arg(taskbar.progress.value)
            Layout.fillWidth: true

            GridLayout {
                columns: 3
                rowSpacing: 10
                columnSpacing: 20
                anchors.fill: parent
                anchors.margins: 10

                Label { text: "Value:" }
                Slider {
                    id: valueSlider

                    value: 82
                    stepSize: 1
                    minimumValue: minSpinBox.value
                    maximumValue: maxSpinBox.value
                    enabled: !indeterminateBox.checked
                    Layout.columnSpan: 2
                }

                Label { text: "Minimum:" }
                SpinBox {
                    id: minSpinBox

                    value: 0
                    stepSize: 1
                    minimumValue: -1000
                    maximumValue: 1000
                    enabled: !indeterminateBox.checked
                    Layout.fillWidth: true
                }

                Button {
                    readonly property string playSymbol: "\u25BA" // BLACK RIGHT-POINTING POINTER
                    readonly property string pauseSymbol: "\u25AE\u25AE" // BLACK VERTICAL RECTANGLE
                    readonly property string stopSymbol: "\u2587" // BLACK SQUARE

                    text: taskbar.progress.stopped ? playSymbol : taskbar.progress.paused ? stopSymbol : pauseSymbol
                    onClicked: taskbar.progress.stopped ? taskbar.progress.resume() : taskbar.progress.paused ? taskbar.progress.stop() : taskbar.progress.pause()
                    Layout.fillHeight: true
                    Layout.rowSpan: 3
                }

                Label { text: "Maximum:" }
                SpinBox {
                    id: maxSpinBox

                    value: 100
                    stepSize: 1
                    minimumValue: -1000
                    maximumValue: 1000
                    enabled: !indeterminateBox.checked
                    Layout.fillWidth: true
                }

                Item { Layout.fillWidth: true }

                CheckBox {
                    id: indeterminateBox
                    text: "Indeterminate"
                }
            }
        }
    }
}
