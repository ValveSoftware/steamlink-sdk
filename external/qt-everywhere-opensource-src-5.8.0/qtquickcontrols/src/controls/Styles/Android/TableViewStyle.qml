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
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles.Android 1.0
import "drawables"

ScrollViewStyle {
    readonly property TableView control: __control

    readonly property color textColor: __label.color
    readonly property color highlightedTextColor: __label.selectedTextColor
    readonly property color backgroundColor: control.backgroundVisible ? AndroidStyle.colorValue(AndroidStyle.styleDef.simple_list_item.defaultBackgroundColor) : "transparent"
    readonly property color alternateBackgroundColor: "transparent"
    readonly property bool activateItemOnSingleClick: true

    property LabelStyle __label: LabelStyle {
        styleDef: AndroidStyle.styleDef.simple_list_item
        visible: false
    }

    property Component headerDelegate: Rectangle {
        color: AndroidStyle.colorValue(styleDef.defaultBackgroundColor)
        height: Math.max(styleDef.View_minHeight || 0, headerLabel.implicitHeight)

        readonly property var styleDef: AndroidStyle.styleDef.listSeparatorTextViewStyle
        readonly property real paddingStart: styleDef.View_paddingStart || styleDef.View_paddingLeft || 0
        readonly property real paddingEnd: styleDef.View_paddingEnd || styleDef.View_paddingRight || 0

        DrawableLoader {
            id: bg
            anchors.fill: parent
            window_focused: control.Window.active
            styleDef: parent.styleDef.View_background
        }

        LabelStyle {
            id: headerLabel
            text: styleData.value !== undefined ? styleData.value : ""
            horizontalAlignment: styleData.textAlignment
            pressed: styleData.pressed
            window_focused: control.Window.active
            styleDef: parent.styleDef
            anchors.fill: parent
            anchors.leftMargin: paddingStart
            anchors.rightMargin: paddingEnd
        }
    }

    property Component rowDelegate: Rectangle {
        readonly property var styleDef: AndroidStyle.styleDef.simple_list_item

        color: styleData.selected && !styleData.pressed ? __label.selectionColor : "transparent"
        height: Math.max(styleDef.View_minHeight || 0, Math.max(bg.implicitHeight, __label.implicitHeight))

        DrawableLoader {
            id: bg
            anchors.fill: parent
            pressed: styleData.pressed
            checked: styleData.selected
            selected: styleData.selected
            window_focused: control.Window.active
            styleDef: AndroidStyle.styleDef.simple_selectable_list_item.View_background
        }

        DrawableLoader {
            id: divider
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            active: styleData.row < control.rowCount - 1
            styleDef: AndroidStyle.styleDef.listViewStyle.ListView_divider
            height: AndroidStyle.styleDef.listViewStyle.ListView_dividerHeight || implicitHeight
        }
    }

    property Component itemDelegate: Item {
        readonly property var styleDef: AndroidStyle.styleDef.simple_list_item

        readonly property real paddingStart: styleDef.View_paddingStart || styleDef.View_paddingLeft || 0
        readonly property real paddingEnd: styleDef.View_paddingEnd || styleDef.View_paddingRight || 0

        height: Math.max(styleDef.View_minHeight || 0, label.implicitHeight)

        LabelStyle {
            id: label
            text: styleData.value !== undefined ? styleData.value : ""
            elide: styleData.elideMode
            horizontalAlignment: styleData.textAlignment
            pressed: styleData.pressed
            focused: control.activeFocus
            selected: styleData.selected
            window_focused: control.Window.active
            styleDef: parent.styleDef
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: paddingStart
            anchors.rightMargin: paddingEnd
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
