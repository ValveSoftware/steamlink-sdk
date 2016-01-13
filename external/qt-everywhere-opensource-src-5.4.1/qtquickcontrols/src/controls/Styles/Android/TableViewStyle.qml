/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
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
