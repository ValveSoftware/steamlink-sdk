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

import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2 as Base
import QtQuick.Controls.Styles.Flat 1.0
import QtQuick.Controls.Private 1.0
import QtQuick.Extras 1.4
import QtQuick.Extras.Private 1.0

Base.SliderStyle {
    handle: Item {
        width: Math.round(26 * FlatStyle.scaleFactor)
        height: width

        readonly property bool focusedOnly: control.activeFocus && !control.pressed

        Rectangle {
            id: handleBorder
            width: parent.width
            height: width
            radius: width / 2
            color: focusedOnly ? FlatStyle.focusedColor :
                   control.hovered && !control.pressed ? FlatStyle.styleColor : "#000000"
            opacity: (control.activeFocus || control.hovered) && !control.pressed ? 1.0 : 0.2
        }

        Rectangle {
            id: handleBody
            readonly property real borderThickness: focusedOnly ? FlatStyle.twoPixels : FlatStyle.onePixel
            x: borderThickness
            y: borderThickness
            width: parent.width - 2 * borderThickness
            height: width
            border.color: "white"
            border.width: (width - parent.width * 10 / 26) / 2
            radius: width / 2
            color: !control.enabled ? FlatStyle.disabledFillColor :
                   focusedOnly ? FlatStyle.focusedColor : FlatStyle.styleColor
        }

        Rectangle {
            id: pressedDarkness
            anchors.fill: parent
            radius: width / 2
            color: "#000000"
            opacity: 0.2
            visible: control.pressed
        }
    }

    groove: Item {
        implicitWidth: Math.round(100 * FlatStyle.scaleFactor)
        height: Math.round(4 * FlatStyle.scaleFactor)
        anchors.verticalCenter: parent.verticalCenter

        Rectangle {
            id: emptyGroove
            width: parent.width
            height: parent.height
            radius: Math.round(2 * FlatStyle.scaleFactor)
            color: "#000000"
            opacity: control.enabled ? 0.2 : 0.12
        }

        Rectangle {
            id: filledGroove
            color: control.enabled ? FlatStyle.styleColor : FlatStyle.mediumFrameColor
            width: styleData.handlePosition
            height: parent.height
            radius: emptyGroove.radius
        }
    }

    // TODO: tickmarks
}
