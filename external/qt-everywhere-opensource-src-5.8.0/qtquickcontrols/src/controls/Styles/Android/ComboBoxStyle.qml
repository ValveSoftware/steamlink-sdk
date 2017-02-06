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

import QtQuick 2.4
import QtQuick.Window 2.2
import QtQuick.Controls 1.2
import QtQuick.Controls.Private 1.0
import QtQuick.Controls.Styles.Android 1.0
import "drawables"

Style {
    id: style

    readonly property ComboBox control: __control
    property int renderType: Text.QtRendering

    padding.left: control.__panel.paddingStart

    property Component panel: Item {
        id: panel

        property bool popup: false
        property int dropDownButtonWidth: paddingEnd * 2 // TODO
        property alias textColor: label.color
        property alias selectionColor: label.selectionColor
        property alias selectedTextColor: label.selectedTextColor
        property alias font: label.font

        readonly property var styleDef: AndroidStyle.styleDef.spinnerStyle
        readonly property var itemDef: AndroidStyle.styleDef.simple_spinner_item

        readonly property real paddingStart: (itemDef.View_paddingStart || itemDef.View_paddingLeft || 0) + bg.padding.left
        readonly property real paddingEnd: (itemDef.View_paddingEnd || itemDef.View_paddingRight || 0) + bg.padding.right

        readonly property real minWidth: styleDef.View_minWidth || 0
        readonly property real minHeight: styleDef.View_minHeight || 0
        readonly property real labelWidth: Math.max(label.implicitWidth, metrics.width) + paddingStart + paddingEnd
        readonly property real labelHeight: label.implicitHeight + bg.padding.top + bg.padding.bottom

        implicitWidth: Math.max(minWidth, Math.max(bg.implicitWidth, labelWidth))
        implicitHeight: Math.max(minHeight, Math.max(bg.implicitHeight, labelHeight))

        DrawableLoader {
            id: bg
            anchors.fill: parent
            pressed: control.pressed
            focused: control.activeFocus
            window_focused: control.Window.active
            styleDef: panel.styleDef.View_background
        }

        TextMetrics {
            id: metrics
            text: "12345678901234567890"
        }

        LabelStyle {
            id: label
            text: control.currentText
            visible: !control.editable
            pressed: control.pressed
            focused: control.activeFocus
            window_focused: control.Window.active
            styleDef: panel.styleDef

            anchors.fill: bg
            anchors.topMargin: bg.padding.top
            anchors.leftMargin: paddingStart
            anchors.rightMargin: paddingEnd
            anchors.bottomMargin: bg.padding.bottom
        }
    }

    property Component __popupStyle: null
    property Component __dropDownStyle: null

    property Component __selectionHandle: DrawableLoader {
        styleDef: AndroidStyle.styleDef.textViewStyle.TextView_textSelectHandleLeft
        x: -width / 4 * 3
        y: styleData.lineHeight
    }

    property Component __cursorHandle: CursorHandleStyle { }
}
