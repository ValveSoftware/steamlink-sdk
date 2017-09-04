/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Virtual Keyboard module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.1

FocusScope {
    id: textBase

    property var editor
    property bool previewTextActive: !editor.activeFocus && text.length === 0
    property int fontPixelSize: 32
    property string previewText
    property int enterKeyAction: EnterKeyAction.None
    property string enterKeyText
    property bool enterKeyEnabled: enterKeyAction === EnterKeyAction.None || editor.text.length > 0 || editor.inputMethodComposing

    implicitHeight: editor.height + 12

    signal enterKeyClicked

    Keys.onReleased: {
        if (event.key === Qt.Key_Return)
            enterKeyClicked()
    }

    Rectangle {
        // background
        radius: 5.0
        anchors.fill: parent
        color: "#FFFFFF"
        border { width: 1; color: editor.activeFocus ? "#5CAA15" : "#BDBEBF" }
    }
    Text {
        id: previewText

        y: 8
        clip: true
        color: "#a0a1a2"
        visible: previewTextActive
        text: textBase.previewText
        font.pixelSize: 28
        anchors { left: parent.left; right: parent.right; margins: 12 }

    }
}
