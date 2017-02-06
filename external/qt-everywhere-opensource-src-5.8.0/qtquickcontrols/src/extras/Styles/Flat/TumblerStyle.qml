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
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4 as Base
import QtQuick.Controls.Private 1.0
import QtQuick.Controls.Styles.Flat 1.0

Base.TumblerStyle {
    id: tumblerStyle

    padding.left: 0
    padding.right: 0
    padding.top: __frameHeight
    padding.bottom: __frameHeight

    visibleItemCount: 5

    readonly property real __frameHeight: FlatStyle.onePixel

    background: null

    foreground: null

    columnForeground: Item {
        Item {
            anchors.centerIn: parent
            width: parent.width
            height: tumblerStyle.__delegateHeight

            Rectangle {
                width: parent.width * 0.8
                anchors.horizontalCenter: parent.horizontalCenter
                height: __frameHeight
                color: control.enabled ? FlatStyle.styleColor : FlatStyle.disabledColor
                opacity: control.enabled ? 1 : 0.2
                anchors.top: parent.top
                visible: !styleData.activeFocus
            }

            Rectangle {
                width: parent.width * 0.8
                anchors.horizontalCenter: parent.horizontalCenter
                height: __frameHeight
                color: control.enabled ? FlatStyle.styleColor : FlatStyle.disabledColor
                opacity: control.enabled ? 1 : 0.2
                anchors.top: parent.bottom
                visible: !styleData.activeFocus
            }
        }
    }

    highlight: Item {
        id: highlightItem
        implicitHeight: (control.height - padding.top - padding.bottom) / tumblerStyle.visibleItemCount

        Rectangle {
            color: styleData.activeFocus ? FlatStyle.highlightColor : "white"
            width: parent.width
            height: parent.height
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    separator: null

    frame: Item {
        Rectangle {
            height: __frameHeight
            width: parent.width
            color: FlatStyle.disabledColor
            opacity: control.enabled ? 0.2 : 0.1
        }

        Rectangle {
            height: __frameHeight
            width: parent.width
            anchors.bottom: parent.bottom
            color: FlatStyle.disabledColor
            opacity: control.enabled ? 0.2 : 0.1
        }
    }

    delegate: Item {
        id: delegateItem
        implicitHeight: (control.height - padding.top - padding.bottom) / tumblerStyle.visibleItemCount

        Text {
            id: label
            text: styleData.value
            color: control.enabled ? (styleData.activeFocus ? FlatStyle.focusedTextColor : FlatStyle.defaultTextColor) : FlatStyle.disabledColor
            opacity: control.enabled ? enabledOpacity : FlatStyle.disabledOpacity
            font.pixelSize: Math.round(TextSingleton.font.pixelSize * 1.3)
            font.family: FlatStyle.fontFamily
            renderType: FlatStyle.__renderType
            anchors.centerIn: parent

            readonly property real enabledOpacity: 1.1 - Math.abs(styleData.displacement * 2) / tumblerStyle.visibleItemCount * (230 / 255)
        }

        Loader {
            id: block
            y: styleData.displacement < 0 ? 0 : (1 - offset) * parent.height
            width: parent.width
            height: parent.height * offset
            clip: true
            active: Math.abs(styleData.displacement) <= 1

            property real offset: Math.max(0, 1 - Math.abs(styleData.displacement))

            sourceComponent: Rectangle {
                // Use a Rectangle that is the same color as the highlight in order to avoid rendering text on top of text.
                color: styleData.activeFocus ? FlatStyle.highlightColor : "white"
                anchors.fill: parent

                Text {
                    id: focusText
                    y: styleData.displacement < 0 ? 0 : parent.height - height
                    width: parent.width
                    height: delegateItem.height
                    color: control.enabled ? (styleData.activeFocus ? "white" : FlatStyle.defaultTextColor) : FlatStyle.disabledColor
                    opacity: control.enabled ? 1 : FlatStyle.disabledOpacity
                    text: styleData.value
                    font.pixelSize: Math.round(TextSingleton.font.pixelSize * 1.5)
                    font.family: FlatStyle.fontFamily
                    renderType: FlatStyle.__renderType
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
