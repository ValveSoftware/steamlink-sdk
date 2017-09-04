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
import QtQuick.Controls.Private 1.0
import QtQuick.Controls.Styles.Flat 1.0

Base.ButtonStyle {
    padding { top: 0; left: 0; right: 0; bottom: 0 }

    readonly property bool __hovered: !Settings.hasTouchScreen && control.hovered

    background: Rectangle {
        property bool down: control.pressed || (control.checkable && control.checked)

        implicitWidth: Math.round(100 * FlatStyle.scaleFactor)
        implicitHeight: Math.round(26 * FlatStyle.scaleFactor)
        radius: FlatStyle.radius

        color: {
            if (control.isDefault) {
                if (control.checkable && control.checked) {
                    if (!control.enabled)
                        return Qt.rgba(0, 0, 0, 0.25);

                    if (control.pressed)
                        return control.activeFocus ? FlatStyle.checkedFocusedAndPressedColor : FlatStyle.checkedAndPressedColorAlt;

                    return control.activeFocus ? FlatStyle.checkedFocusedAndPressedColor : FlatStyle.pressedColor;
                } else {
                    // Normal state.
                    if (!control.enabled)
                        return Qt.rgba(0, 0, 0, 0.15);

                    if (control.pressed)
                        return control.activeFocus ? FlatStyle.checkedFocusedAndPressedColor : FlatStyle.pressedColor;

                    if (control.activeFocus)
                        return control.pressed ? FlatStyle.checkedFocusedAndPressedColor : FlatStyle.focusedColor;

                    return FlatStyle.styleColor;
                }
            }

            // Non-default button.
            if (control.checkable && control.checked) {
                if (!control.enabled)
                    return Qt.rgba(0, 0, 0, 0.1);

                if (control.pressed)
                    return control.activeFocus ? FlatStyle.checkedFocusedAndPressedColor : FlatStyle.checkedAndPressedColor;

                return control.activeFocus ? FlatStyle.focusedAndPressedColor : FlatStyle.pressedColorAlt;
            }

            if (control.pressed)
                return control.activeFocus ? FlatStyle.focusedAndPressedColor : FlatStyle.pressedColorAlt;

            if (!control.activeFocus && __hovered)
                return FlatStyle.hoveredColor;

            return "transparent";
        }

        border.color: {
            if (!control.isDefault) {
                if (!control.enabled)
                    return Qt.rgba(0, 0, 0, (control.checkable && control.checked ? 0.3 : 0.2));

                if (control.activeFocus && !control.pressed && !control.checked)
                    return FlatStyle.focusedColor;

                if (!__hovered && !control.checked && !control.pressed)
                    return FlatStyle.styleColor;
            }

            return "transparent";
        }

        border.width: control.activeFocus ? FlatStyle.twoPixels : FlatStyle.onePixel
    }

    label: Item {
        readonly property bool isDown: control.pressed || (control.checkable && control.checked)
        readonly property bool hasIcon: icon.status === Image.Ready || icon.status === Image.Loading
        readonly property bool hasMenu: !!control.menu
        readonly property bool hasText: !!control.text
        readonly property int labelSpacing: Math.round(10 * FlatStyle.scaleFactor)
        implicitWidth: icon.implicitWidth + label.implicitWidth + indicator.implicitHeight
                       + (hasIcon || hasText ? labelSpacing : 0) + (hasIcon && hasText ? labelSpacing : 0)
        implicitHeight: Math.max(Math.max(icon.implicitHeight, label.implicitHeight), indicator.height)
        baselineOffset: label.y + label.baselineOffset
        Image {
            id: icon
            visible: hasIcon
            source: control.iconSource
            // center align when only icon, otherwise left align
            anchors.left: parent.left
            anchors.leftMargin: hasMenu || hasText ? labelSpacing : parent.width / 2 - width / 2
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            id: label
            visible: hasText
            text: control.text
            elide: Text.ElideRight
            font.family: FlatStyle.fontFamily
            renderType: FlatStyle.__renderType
            opacity: !enabled && !control.isDefault ? FlatStyle.disabledOpacity : 1.0
            color: control.isDefault ? FlatStyle.selectedTextColor :
                   !enabled ? FlatStyle.disabledColor :
                   isDown ? FlatStyle.selectedTextColor :
                   control.activeFocus ? FlatStyle.focusedTextColor :
                   __hovered ? FlatStyle.selectedTextColor : FlatStyle.styleColor
            horizontalAlignment: hasMenu != hasIcon ? Qt.AlignLeft : Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            anchors.fill: parent
            anchors.leftMargin: labelSpacing + (hasIcon ? labelSpacing + icon.width : 0)
            anchors.rightMargin: hasMenu ? indicator.width : labelSpacing
        }
        Item {
            id: indicator
            visible: hasMenu
            implicitWidth: Math.round(28 * FlatStyle.scaleFactor)
            implicitHeight: Math.round(26 * FlatStyle.scaleFactor)
            // center align when only menu, otherwise right align
            anchors.right: parent.right
            anchors.rightMargin: !hasIcon && !hasText ? parent.width / 2 - width / 2 : 0
            anchors.verticalCenter: parent.verticalCenter

            opacity: control.enabled ? 1.0 : 0.2
            property color color: !control.enabled ? FlatStyle.disabledColor :
                                   control.activeFocus && !control.pressed ? FlatStyle.focusedColor :
                                   control.activeFocus || control.pressed || __hovered ? FlatStyle.selectedTextColor : FlatStyle.styleColor

            Rectangle {
                x: Math.round(7 * FlatStyle.scaleFactor)
                y: Math.round(7 * FlatStyle.scaleFactor)
                width: Math.round(14 * FlatStyle.scaleFactor)
                height: FlatStyle.twoPixels
                color: indicator.color
            }
            Rectangle {
                x: Math.round(7 * FlatStyle.scaleFactor)
                y: Math.round(12 * FlatStyle.scaleFactor)
                width: Math.round(14 * FlatStyle.scaleFactor)
                height: FlatStyle.twoPixels
                color: indicator.color
            }
            Rectangle {
                x: Math.round(7 * FlatStyle.scaleFactor)
                y: Math.round(17 * FlatStyle.scaleFactor)
                width: Math.round(14 * FlatStyle.scaleFactor)
                height: FlatStyle.twoPixels
                color: indicator.color
            }
        }
    }
}
