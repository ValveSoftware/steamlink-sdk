/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Extras module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Controls.Private 1.0
import QtQuick.Controls.Styles.Flat 1.0

// Internal, for use with CheckBoxStyle and GroupBoxStyle
Item {
    implicitWidth: Math.round(26 * FlatStyle.scaleFactor)
    implicitHeight: implicitWidth

    property bool controlEnabled: false
    property bool controlActiveFocus: false
    property bool controlPressed: false
    property bool controlHovered: false
    property bool controlChecked: false
    property alias backgroundVisible: background.visible

    onControlActiveFocusChanged: checkCanvas.requestPaint()
    onControlEnabledChanged: checkCanvas.requestPaint()
    onControlPressedChanged: checkCanvas.requestPaint()

    Rectangle {
        id: background
        anchors.centerIn: parent
        width: Math.round(20 * FlatStyle.scaleFactor)
        height: width
        radius: FlatStyle.radius
        color: controlEnabled ? (controlPressed ? FlatStyle.lightFrameColor : FlatStyle.backgroundColor) : FlatStyle.disabledFillColor
        border.color: !controlEnabled ? FlatStyle.disabledFillColor :
            (controlPressed ? FlatStyle.darkFrameColor :
            (controlActiveFocus ? FlatStyle.focusedColor :
            (controlHovered ? FlatStyle.styleColor : FlatStyle.lightFrameColor)))
        border.width: controlActiveFocus &&
            !controlPressed ? FlatStyle.twoPixels : FlatStyle.onePixel
    }

    Canvas {
        id: checkCanvas
        anchors.centerIn: parent
        width: Math.round(20 * FlatStyle.scaleFactor)
        height: width
        visible: controlChecked

        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();

            ctx.beginPath();
            ctx.moveTo(0.417 * width, 0.783 * height);
            ctx.lineTo(0.152 * width, 0.519 * height);
            ctx.lineTo(0.248 * width, 0.423 * height);
            ctx.lineTo(0.417 * width, 0.593 * height);
            ctx.lineTo(0.75 * width, 0.26 * height);
            ctx.lineTo(0.846 * width, 0.355 * height);
            ctx.lineTo();
            ctx.closePath();
            ctx.fillStyle = !controlEnabled ? FlatStyle.mediumFrameColor :
                (controlPressed ? FlatStyle.checkedAndPressedColor :
                (controlActiveFocus ? FlatStyle.focusedColor : FlatStyle.styleColor));
            ctx.fill();
        }
    }

}
