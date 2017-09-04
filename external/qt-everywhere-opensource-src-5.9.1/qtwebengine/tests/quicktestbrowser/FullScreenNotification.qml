/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

import QtQuick 2.5

Rectangle {
    id: fullScreenNotification
    width: 500
    height: 40
    color: "white"
    radius: 7

    visible: false
    opacity: 0

    function show() {
        visible = true
        opacity = 1
        reset.start()
    }

    function hide() {
        reset.stop()
        opacity = 0
    }

    Behavior on opacity {
        NumberAnimation {
            duration: 750
            onStopped: {
                if (opacity == 0)
                    visible = false
            }
        }
    }

    Timer {
        id: reset
        interval: 5000
        onTriggered: hide()
    }

    anchors.horizontalCenter: parent.horizontalCenter
    y: 125

    Text {
        id: message
        width: parent.width

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter

        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter

        wrapMode: Text.WordWrap
        elide: Text.ElideNone
        clip: true

        text: qsTr("You are now in fullscreen mode. Press ESC to quit!")
    }
}
