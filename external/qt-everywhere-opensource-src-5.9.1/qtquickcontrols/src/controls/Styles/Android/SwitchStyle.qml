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
