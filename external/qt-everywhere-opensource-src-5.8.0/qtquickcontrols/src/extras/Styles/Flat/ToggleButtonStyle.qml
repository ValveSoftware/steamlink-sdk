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
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4 as Base
import QtQuick.Controls.Styles.Flat 1.0
import QtQuick.Controls.Private 1.0
import QtQuick.Extras 1.4
import QtQuick.Extras.Private 1.0

Base.ToggleButtonStyle {
    label: Label {
        text: control.text
        color: FlatStyle.darkFrameColor
        font.family: FlatStyle.fontFamily
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        renderType: FlatStyle.__renderType
    }

    background: Item {
        implicitWidth: Math.round(100 * FlatStyle.scaleFactor)
        implicitHeight: Math.round(52 * FlatStyle.scaleFactor)

        readonly property real outerLightLineWidth: Math.max(4, Math.round(innerRect.radius * 0.5))
        readonly property real thinDarkLineWidth: Math.max(1, FlatStyle.onePixel)
        readonly property color indicatorColor: control.enabled ? (control.checked ? FlatStyle.styleColor : FlatStyle.invalidColor) : FlatStyle.lightFrameColor
        readonly property bool hovered: control.hovered && (!Settings.hasTouchScreen && !Settings.isMobile)

        Rectangle {
            id: innerRect
            color: control.enabled ? (control.pressed ? FlatStyle.lightFrameColor : "white") : Qt.rgba(0, 0, 0, 0.07)
            border.color: control.enabled ? (hovered ? indicatorColor : FlatStyle.mediumFrameColor) : "#d8d8d8" // TODO: specs don't mention having a border for disabled toggle buttons
            border.width: !control.activeFocus ? thinDarkLineWidth : 0
            radius: Math.round(3 * FlatStyle.scaleFactor)
            anchors.fill: parent

            Item {
                id: clipItem
                x: control.checked ? width : 0
                width: indicatorRect.width / 2
                height: indicatorRect.height / 2
                clip: true

                Rectangle {
                    id: indicatorRect
                    x: control.checked ? -parent.width : 0
                    color: indicatorColor
                    radius: innerRect.radius
                    width: innerRect.width
                    height: Math.round(8 * FlatStyle.scaleFactor) * 2
                }
            }
        }

        Rectangle {
            anchors.fill: innerRect
            color: "transparent"
            radius: innerRect.radius
            border.color: FlatStyle.focusedColor
            border.width: 2 * FlatStyle.scaleFactor
            visible: control.activeFocus
        }
    }
}
