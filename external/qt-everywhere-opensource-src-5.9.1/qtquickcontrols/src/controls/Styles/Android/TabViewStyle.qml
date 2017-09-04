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

Style {
    readonly property TabView control: __control

    readonly property bool tabsMovable: false
    readonly property int tabsAlignment: Qt.AlignLeft
    readonly property int tabOverlap: 0
    readonly property int frameOverlap: 0

    property Component frame: null
    property Component leftCorner: null
    property Component rightCorner: null

    property Component tabBar: Item {
        id: panel

        readonly property var styleDef: AndroidStyle.styleDef.actionBarStyle

        readonly property real minWidth: styleDef.View_minWidth || 0
        readonly property real minHeight: styleDef.View_minHeight || 0

        implicitWidth: Math.max(minWidth, loader.implicitWidth)
        implicitHeight: Math.max(minHeight, loader.implicitHeight)

        DrawableLoader {
            id: loader
            anchors.fill: parent
            focused: control.activeFocus
            window_focused: focused && control.Window.active
            styleDef: panel.styleDef.ActionBar_backgroundStacked
        }
    }

    property Component tab: Item {
        id: tab

        readonly property real minHeight: AndroidStyle.styleDef.actionButtonStyle.View_minHeight || 0
        readonly property real contentHeight: label.implicitHeight + bg.padding.top + bg.padding.bottom

        readonly property real dividerPadding: AndroidStyle.styleDef.actionBarTabBarStyle.LinearLayout_dividerPadding

        implicitWidth: styleData.availableWidth / Math.max(1, control.count)
        implicitHeight: Math.max(minHeight, Math.max(bg.implicitHeight, contentHeight))

        DrawableLoader {
            id: bg
            anchors.fill: parent
            enabled: styleData.enabled
            pressed: styleData.pressed
            selected: styleData.selected
            focused: styleData.activeFocus
            window_focused: focused && control.Window.active
            styleDef: AndroidStyle.styleDef.actionBarTabStyle.View_background
        }

        DrawableLoader {
            id: divider
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            enabled: styleData.enabled
            pressed: styleData.pressed
            selected: styleData.selected
            focused: styleData.activeFocus
            window_focused: focused && control.Window.active
            styleDef: AndroidStyle.styleDef.actionBarTabBarStyle.LinearLayout_divider
            visible: styleData.index < control.count - 1 && control.count > 1
        }

        LabelStyle {
            id: label
            text: styleData.title
            pressed: styleData.pressed
            enabled: styleData.enabled
            focused: styleData.activeFocus
            selected: styleData.selected
            window_focused: control.Window.active
            styleDef: AndroidStyle.styleDef.actionBarTabTextStyle

            anchors.fill: bg
            anchors.topMargin: bg.padding.top
            anchors.leftMargin: bg.padding.left + dividerPadding
            anchors.rightMargin: bg.padding.right + dividerPadding
            anchors.bottomMargin: bg.padding.bottom
        }
    }
}
