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
