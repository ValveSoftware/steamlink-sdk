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
