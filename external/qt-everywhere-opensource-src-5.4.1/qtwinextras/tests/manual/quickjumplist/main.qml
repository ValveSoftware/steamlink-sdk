/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
