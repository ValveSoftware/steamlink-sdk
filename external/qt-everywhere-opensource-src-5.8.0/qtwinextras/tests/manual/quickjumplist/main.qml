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
import QtQuick.Dialogs 1.0
import QtQuick.Layouts 1.0
import QtQuick.Controls 1.0

ApplicationWindow {
    id: window

    title: "JumpList"

    width: 800
    height: 400
    minimumWidth: groupBox.implicitWidth + 40
    minimumHeight: groupBox.implicitHeight + 40

    JumpList {
        id: jumpList
        recent.visible: recentBox.checked
        frequent.visible: frequentBox.checked
        tasks: JumpListCategory {
            visible: tasksBox.checked
            JumpListLink {
                title: "qmlscene"
                description: "qmlscene main.qml"
                executablePath: Qt.application.arguments[0]
                arguments: Qt.application.arguments[1]
            }
        }
        JumpListCategory {
            title: "Custom"
            visible: customBox.checked
            JumpListLink {
                title: "qmlscene"
                description: "qmlscene main.qml"
                executablePath: Qt.application.arguments[0]
                arguments: Qt.application.arguments[1]
            }
        }
    }

    GroupBox {
        id: groupBox
        title: "JumpList"
        anchors.centerIn: parent
        RowLayout {
            anchors.fill: parent
            anchors.margins: 20
            ColumnLayout {
                CheckBox {
                    id: recentBox
                    text: "Recent"
                }
                CheckBox {
                    id: frequentBox
                    text: "Frequent"
                }
                CheckBox {
                    id: tasksBox
                    text: "Tasks"
                }
                CheckBox {
                    id: customBox
                    text: "Custom"
                }
            }
            Button {
                text: "Open..."
                onClicked: dialog.open()
                FileDialog {
                    id: dialog
                }
            }
        }
    }
}
