/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Extras module of the Qt Toolkit.
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
import QtQuick.Controls.Private 1.0 as Private
import QtQuick.Controls.Styles.Flat 1.0

Private.GroupBoxStyle {
    id: root

    readonly property int spacing: Math.round(8 * FlatStyle.scaleFactor)

    padding {
        left: spacing
        right: spacing
        bottom: spacing
    }

    textColor: !control.enabled ? FlatStyle.disabledColor : check.activeFocus ? FlatStyle.focusedTextColor : FlatStyle.defaultTextColor

    panel: Item {
        anchors.fill: parent

        Rectangle {
            id: background
            radius: FlatStyle.radius
            border.width: control.flat ? 0 : FlatStyle.onePixel
            border.color: FlatStyle.lightFrameColor
            color: control.flat ? FlatStyle.flatFrameColor : "transparent"
            anchors.fill: parent
            anchors.topMargin: Math.max(indicator.height, label.height) + root.spacing / 2
        }

        // TODO:
        Binding {
            target: root.padding
            property: "top"
            value: background.anchors.topMargin + root.spacing
        }

        CheckBoxDrawer {
            id: indicator
            visible: control.checkable
            controlEnabled: control.enabled
            controlActiveFocus: check.activeFocus
            controlPressed: check.pressed
            controlChecked: control.checked
            anchors.left: parent.left
            // compensate padding around check indicator
            anchors.leftMargin: Math.round(-3 * FlatStyle.scaleFactor)
        }

        Text {
            id: label
            anchors.left: control.checkable ? indicator.right : parent.left
            anchors.right: parent.right
            anchors.verticalCenter: indicator.verticalCenter
            anchors.leftMargin: control.checkable ? root.spacing / 2 : 0
            text: control.title
            color: root.textColor
            renderType: FlatStyle.__renderType
            elide: Text.ElideRight
            font.family: FlatStyle.fontFamily
            font.pixelSize: 16 * FlatStyle.scaleFactor
        }
    }
}
