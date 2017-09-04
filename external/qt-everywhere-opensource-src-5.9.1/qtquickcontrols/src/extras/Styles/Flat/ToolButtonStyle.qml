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

Base.ButtonStyle {
    padding { top: 0; left: 0; right: 0; bottom: 0 }

    panel: Item {
        id: panelItem

        readonly property bool isDown: control.pressed || (control.checkable && control.checked)
        readonly property bool hasIcon: icon.status === Image.Ready || icon.status === Image.Loading
        readonly property bool hasMenu: !!control.menu
        readonly property bool hasText: !!control.text
        readonly property real margins: 10 * FlatStyle.scaleFactor

        ToolButtonBackground {
            id: background
            anchors.fill: parent
            buttonEnabled: control.enabled
            buttonHasActiveFocus: control.activeFocus
            buttonPressed: control.pressed
            buttonChecked: control.checkable && control.checked
            buttonHovered: !Settings.hasTouchScreen && control.hovered
        }

        implicitWidth: icon.implicitWidth + label.implicitWidth + indicator.implicitHeight
                       + (hasIcon || hasText ? panelItem.margins : 0) + (hasIcon && hasText ? panelItem.margins : 0)
        implicitHeight: Math.max(background.height, Math.max(icon.implicitHeight, Math.max(label.implicitHeight, indicator.height)))
        baselineOffset: label.y + label.baselineOffset

        Image {
            id: icon
            visible: hasIcon
            source: control.iconSource
            anchors.leftMargin: panelItem.margins
            anchors.verticalCenter: parent.verticalCenter
            // center align when only icon, otherwise left align
            anchors.left: hasMenu || hasText ? parent.left : undefined
            anchors.horizontalCenter: !hasMenu && !hasText ? parent.horizontalCenter : undefined
        }
        Text {
            id: label
            visible: hasText
            text: control.text
            elide: Text.ElideRight
            font.family: FlatStyle.fontFamily
            renderType: FlatStyle.__renderType
            color: !enabled ? FlatStyle.disabledColor : panelItem.isDown && control.activeFocus
                    ? FlatStyle.selectedTextColor : control.activeFocus
                    ? FlatStyle.focusedColor : FlatStyle.defaultTextColor
            opacity: !enabled ? FlatStyle.disabledOpacity : 1.0
            horizontalAlignment: hasMenu && !hasIcon ? Qt.AlignLeft : Qt.AlignHCenter
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: icon.right
            anchors.right: indicator.left
            anchors.leftMargin: hasIcon ? panelItem.margins : 0
            anchors.rightMargin: hasMenu ? 0 : panelItem.margins
        }
        ToolButtonIndicator {
            id: indicator
            visible: panelItem.hasMenu
            buttonEnabled: control.enabled
            buttonHasActiveFocus: control.activeFocus
            buttonPressedOrChecked: panelItem.isDown
            anchors.verticalCenter: parent.verticalCenter
            // center align when only menu, otherwise right align
            anchors.right: hasIcon || hasText ? parent.right : undefined
            anchors.horizontalCenter: !hasIcon && !hasText ? parent.horizontalCenter : undefined
        }
    }
}
