/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.2
import QtQuick.Controls.Private 1.0
import QtQuick.Controls.Styles.Android 1.0
import "drawables"

Style {
    readonly property Button control: __control

    property Component panel: Item {
        id: panel

        readonly property var styleDef: control.checkable ? AndroidStyle.styleDef.buttonStyleToggle
                                                          : AndroidStyle.styleDef.buttonStyle

        readonly property real minWidth: styleDef.View_minWidth || 0
        readonly property real minHeight: styleDef.View_minHeight || 0
        readonly property real contentWidth: row.implicitWidth + bg.padding.left + bg.padding.right
        readonly property real contentHeight: row.implicitHeight + bg.padding.top + bg.padding.bottom

        readonly property bool hasIcon: icon.status === Image.Ready || icon.status === Image.Loading

        implicitWidth: Math.max(minWidth, Math.max(bg.implicitWidth, contentWidth))
        implicitHeight: Math.max(minHeight, Math.max(bg.implicitHeight, contentHeight))

        DrawableLoader {
            id: bg
            anchors.fill: parent
            pressed: control.pressed
            checked: control.checked
            focused: control.activeFocus
            window_focused: control.Window.active
            styleDef: panel.styleDef.View_background
        }

        RowLayout {
            id: row
            anchors.fill: parent
            anchors.topMargin: bg.padding.top
            anchors.leftMargin: bg.padding.left
            anchors.rightMargin: bg.padding.right
            anchors.bottomMargin: bg.padding.bottom
            spacing: Math.max(bg.padding.left, bg.padding.right)

            Image {
                id: icon
                visible: hasIcon
                source: control.iconSource
                Layout.alignment: Qt.AlignCenter
            }

            LabelStyle {
                id: label
                visible: !!text
                text: StyleHelpers.removeMnemonics(control.text)
                pressed: control.pressed
                focused: control.activeFocus
                selected: control.checked
                window_focused: control.Window.active
                styleDef: panel.styleDef
                Layout.fillWidth: true
            }
        }
    }
}
