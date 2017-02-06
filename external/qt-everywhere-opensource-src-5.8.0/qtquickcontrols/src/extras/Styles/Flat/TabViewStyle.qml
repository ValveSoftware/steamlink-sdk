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
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2 as Base
import QtQuick.Controls.Styles.Flat 1.0
import QtQuick.Controls.Private 1.0

Base.TabViewStyle {
    readonly property int frameWidth: Math.round(FlatStyle.scaleFactor)

    tabOverlap: -frameWidth
    frameOverlap: frameWidth

    frame: Rectangle {
        visible: control.frameVisible
        color: FlatStyle.backgroundColor
        border.color: FlatStyle.lightFrameColor
    }

    tab: Item {
        readonly property int totalWidth: styleData.availableWidth + tabOverlap * (control.count - 1)
        readonly property int tabWidth: Math.max(1, totalWidth / Math.max(1, control.count))
        readonly property int remainder: (styleData.index == control.count - 1 && tabWidth > 0) ? totalWidth % tabWidth : 0

        implicitWidth: tabWidth + remainder
        implicitHeight: Math.round(40 * FlatStyle.scaleFactor)

        Rectangle {
            anchors.fill: parent
            visible: styleData.pressed || !styleData.selected || styleData.activeFocus
            opacity: styleData.enabled ? 1.0 : FlatStyle.disabledOpacity
            color: styleData.activeFocus ? (styleData.pressed ? FlatStyle.checkedFocusedAndPressedColor : FlatStyle.focusedColor) :
                    styleData.pressed ? FlatStyle.pressedColor :
                    styleData.selected ? FlatStyle.backgroundColor :
                   !styleData.enabled ? FlatStyle.disabledColor : FlatStyle.styleColor
        }

        Text {
            text: styleData.title
            anchors.fill: parent
            anchors.leftMargin: Math.round(5 * FlatStyle.scaleFactor)
            anchors.rightMargin: anchors.leftMargin
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.family: FlatStyle.fontFamily
            renderType: FlatStyle.__renderType
            elide: Text.ElideRight
            opacity: !styleData.enabled && styleData.selected ? FlatStyle.disabledOpacity : 1.0
            color: !styleData.enabled && styleData.selected ? FlatStyle.disabledColor :
                    styleData.selected && styleData.enabled && !styleData.activeFocus && !styleData.pressed ? FlatStyle.styleColor : FlatStyle.selectedTextColor
        }
    }

    tabBar: Rectangle {
        color: FlatStyle.backgroundColor
        border.color: control.frameVisible ? FlatStyle.lightFrameColor : "transparent"
        anchors.fill: parent
        Rectangle {
            height: frameWidth
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: frameWidth
            y: control.tabPosition == Qt.TopEdge ? parent.height - height : 0
            color: FlatStyle.backgroundColor
            visible: control.frameVisible
        }
    }
}
