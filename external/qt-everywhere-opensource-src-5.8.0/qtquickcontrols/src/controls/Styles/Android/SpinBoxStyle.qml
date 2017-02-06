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

    readonly property SpinBox control: __control

    property int renderType: Text.QtRendering

    property Component panel: Item {
        id: panel

        readonly property var styleDef: AndroidStyle.styleDef.editTextStyle

        readonly property real contentWidth: Math.max(styleDef.View_minWidth || 0, styleData.contentWidth)
        readonly property real contentHeight: Math.max(styleDef.View_minHeight || 0, styleData.contentHeight)
        readonly property real labelWidth: Math.max(label.implicitWidth, metrics.width) + bg.padding.left + bg.padding.right
        readonly property real labelHeight: label.implicitHeight + bg.padding.top + bg.padding.bottom

        implicitWidth: Math.max(contentWidth, Math.max(bg.implicitWidth, labelWidth))
        implicitHeight: Math.max(contentHeight, Math.max(bg.implicitHeight, labelHeight))

        DrawableLoader {
            id: bg
            anchors.fill: parent
            focused: control.activeFocus
            window_focused: focused && control.Window.active
            styleDef: panel.styleDef.View_background
        }

        Binding { target: style; property: "padding.top"; value: bg.padding.top }
        Binding { target: style; property: "padding.left"; value: bg.padding.left }
        Binding { target: style; property: "padding.right"; value: bg.padding.right }
        Binding { target: style; property: "padding.bottom"; value: bg.padding.bottom }

        readonly property alias font: label.font
        readonly property alias foregroundColor: label.color
        readonly property alias selectionColor: label.selectionColor
        readonly property alias selectedTextColor: label.selectedTextColor

        readonly property rect upRect: Qt.rect(0, 0, 0, 0)
        readonly property rect downRect: Qt.rect(0, 0, 0, 0)

        readonly property int horizontalAlignment: Qt.AlignLeft
        readonly property int verticalAlignment: Qt.AlignVCenter

        TextMetrics {
            id: metrics
            text: "12345678901234567890"
        }

        LabelStyle {
            id: label
            visible: false
            text: control.__text
            focused: control.activeFocus
            window_focused: focused && control.Window.active
            styleDef: panel.styleDef
        }
    }

    property Component __selectionHandle: DrawableLoader {
        styleDef: AndroidStyle.styleDef.textViewStyle.TextView_textSelectHandleLeft
        x: -width / 4 * 3
        y: styleData.lineHeight
    }

    property Component __cursorHandle: CursorHandleStyle { }
}
