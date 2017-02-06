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

import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

Rectangle {
    property alias zoomFactor: slider.value ;
    function zoomIn() {
        visible = true
        visibilityTimer.restart()
        zoomFactor = zoomFactor + 0.25;
    }
    function zoomOut() {
        visible = true
        visibilityTimer.restart()
        zoomFactor = zoomFactor - 0.25;
    }
    function reset() { zoomFactor = 1.0 }

    width: 220
    height: 30
    color: palette.window
    visible: false
    radius: 4

    SystemPalette {
        id: palette
    }
    Timer {
        id: visibilityTimer
        interval: 3000
        repeat: false
        onTriggered: zoomController.visible = false
    }

    RowLayout {
        anchors.margins: 4
        anchors.fill: parent
        ToolButton {
            id: plusButton
            text: '+'
            onClicked: zoomIn()
        }
        ToolButton {
            text: '\u2014'
            id: minusButton
            onClicked: zoomOut()
        }
        Slider {
            id: slider
            maximumValue: 5.0
            minimumValue: 0.25
            Layout.fillWidth: true;
            stepSize: 0.05
            value: 1
            onValueChanged: visibilityTimer.restart()
        }
        Button {
            text: "Reset"
            onClicked: reset()
        }
    }
}
