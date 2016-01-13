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
import QtQuick.Controls.Styles 1.2
import QtQuick.Controls.Styles.Android 1.0
import "drawables"

SwitchStyle {
    property Switch control: __control
    readonly property bool hasHoloSwitch: !!AndroidStyle.styleDef.switchStyle

    Component.onCompleted: if (hasHoloSwitch) panel = holoPanel

    Component {
        id: holoPanel

        Item {
            id: panel

            readonly property var styleDef: AndroidStyle.styleDef.switchStyle

            implicitWidth: Math.max(styleDef.Switch_switchMinWidth, Math.max(track.implicitWidth, 2 * thumb.implicitWidth))
            implicitHeight: Math.max(track.implicitHeight, thumb.implicitHeight)

            property real min: track.padding.left
            property real max: track.width - thumb.width - track.padding.left
            property var __handle: thumb

            DrawableLoader {
                id: track
                anchors.fill: parent
                pressed: control.pressed
                checked: control.checked
                focused: control.activeFocus
                window_focused: control.Window.active
                styleDef: panel.styleDef.Switch_track
            }

            Item {
                id: thumb

                readonly property bool hideText: AndroidStyle.styleDef.switchStyle.Switch_showText === false

                x: control.checked ? max : min

                TextMetrics {
                    id: onMetrics
                    font: label.font
                    text: panel.styleDef.Switch_textOn
                }

                TextMetrics {
                    id: offMetrics
                    font: label.font
                    text: panel.styleDef.Switch_textOff
                }

                readonly property real maxTextWidth: Math.max(onMetrics.width, offMetrics.width)

                implicitWidth: Math.max(loader.implicitWidth, maxTextWidth + 2 * panel.styleDef.Switch_thumbTextPadding)
                implicitHeight: Math.max(loader.implicitHeight, onMetrics.height, offMetrics.height)

                anchors.top: parent.top
                anchors.bottom: parent.bottom

                Behavior on x {
                    id: behavior
                    enabled: thumb.status === Loader.Ready
                    NumberAnimation {
                        duration: 150
                        easing.type: Easing.OutCubic
                    }
                }

                DrawableLoader {
                    id: loader
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: -padding.left
                    anchors.rightMargin: -padding.right
                    anchors.verticalCenter: parent.verticalCenter
                    pressed: control.pressed
                    checked: control.checked
                    focused: control.activeFocus
                    window_focused: control.Window.active
                    styleDef: panel.styleDef.Switch_thumb
                }

                LabelStyle {
                    id: label
                    visible: !thumb.hideText
                    text: control.checked ? panel.styleDef.Switch_textOn : panel.styleDef.Switch_textOff

                    pressed: control.pressed
                    focused: control.activeFocus
                    selected: control.checked
                    window_focused: control.Window.active
                    styleDef: panel.styleDef.Switch_switchTextAppearance

                    anchors.fill: parent
                }
            }
        }
    }
}
