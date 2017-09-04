/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Graphical Effects module.
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

Image {
    id: positionPicker
    property real xValue: 0
    property real yValue: 0

    x: parent.width * xValue - positionPicker.width / 2
    y: parent.height * yValue - positionPicker.height / 2

    property alias pressed: mouseArea.pressed

    source: pressed ? "images/slider_handle_pressed.png" : "images/slider_handle.png"

    MouseArea {
        id: mouseArea

        property real startX
        property real startY

        anchors.fill: parent
        onPressed: {
            startX = mouseX;
            startY = mouseY;
        }

        onPositionChanged: {
            xValue = (positionPicker.x + positionPicker.width / 2 + mouseX - startX) / positionPicker.parent.width;
            yValue = (positionPicker.y + positionPicker.height / 2 + mouseY - startY) / positionPicker.parent.height;
        }
    }
}
