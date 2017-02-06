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

import QtQuick 2.0

Item {

    Rectangle {
        id: helpbutton; anchors { right: parent.right; bottom: parent.bottom }
        height: 30; width: 30; color: "lightgray"; radius: 5; visible: qmlfiletoload == ""
        Text { text: "?"; anchors.centerIn: parent; font.pointSize: 12 }
        MouseArea { anchors.fill: parent; onClicked: { elementsapp.qmlfiletoload = "Help.qml" } }
    }

    Rectangle {
        width: parent.width - (20 + helpbutton.width); height: infotext.height; radius: 5; opacity: .7; visible: infotext.text != ""
        anchors { right: helpbutton.left; bottom: parent.bottom; rightMargin: 5; bottomMargin: 20 }
        Text { id: infotext; text: elementsapp.helptext; width: parent.width - 10; anchors.centerIn: parent; wrapMode: Text.WordWrap }
    }
}
