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
import QtQuick.Controls.Private 1.0
import QtQuick.Controls.Styles.Android 1.0
import "drawables"

GroupBoxStyle {
    id: root

    panel: Item {
        anchors.fill: parent

        readonly property var styleDef: AndroidStyle.styleDef.checkboxStyle

        readonly property real contentMargin: label.implicitHeight / 3
        readonly property real topMargin: control.checkable ? indicator.height : label.height
        Binding { target: root; property: "padding.top"; value: topMargin + contentMargin }
        Binding { target: root; property: "padding.left"; value: contentMargin }
        Binding { target: root; property: "padding.right"; value: contentMargin }
        Binding { target: root; property: "padding.bottom"; value: contentMargin }

        DrawableLoader {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: topMargin
            styleDef: AndroidStyle.styleDef.actionBarStyle.ActionBar_divider
        }

        DrawableLoader {
            active: !control.flat
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.topMargin: topMargin
            styleDef: AndroidStyle.styleDef.actionBarStyle.ActionBar_divider
        }

        DrawableLoader {
            active: !control.flat
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.topMargin: topMargin
            styleDef: AndroidStyle.styleDef.actionBarStyle.ActionBar_divider
        }

        DrawableLoader {
            active: !control.flat
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            styleDef: AndroidStyle.styleDef.actionBarStyle.ActionBar_divider
        }

        DrawableLoader {
            id: indicator
            active: control.checkable
            checked: control.checked
            pressed: check.pressed
            focused: check.activeFocus
            window_focused: control.Window.active
            styleDef: AndroidStyle.styleDef.checkboxStyle.CompoundButton_button
            width: control.checkable ? item.implicitWidth : 0
            anchors.verticalCenter: parent.top
            anchors.verticalCenterOffset: topMargin / 2
        }

        LabelStyle {
            id: label
            text: control.title
            pressed: check.pressed
            selected: control.checked
            focused: check.activeFocus
            window_focused: control.Window.active
            styleDef: AndroidStyle.styleDef.checkboxStyle

            anchors.left: indicator.right
            anchors.right: parent.right
            anchors.verticalCenter: parent.top
            anchors.verticalCenterOffset: topMargin / 2
        }
    }
}
