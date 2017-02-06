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

import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Controls.Styles 1.2 as Base
import QtQuick.Controls.Private 1.0
import QtQuick.Controls.Styles.Flat 1.0

Base.ScrollViewStyle {
    readonly property TableView control: __control

    readonly property color backgroundColor: "transparent"
    readonly property color alternateBackgroundColor: FlatStyle.disabledColor

    readonly property color textColor: FlatStyle.defaultTextColor
    readonly property color highlightedTextColor: FlatStyle.styleColor

    transientScrollBars: true

    readonly property bool activateItemOnSingleClick: false

    readonly property real __alternateBackgroundOpacity: 0.07
    readonly property real __selectedBackgroundOpacity: 0.2
    readonly property real __focusedBackgroundOpacity: 0.4
    readonly property real __columnMargin: Math.round(20 * FlatStyle.scaleFactor)

    frame: Item {
        visible: control.frameVisible && control.alternatingRowColors
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: FlatStyle.mediumFrameColor
        }
    }

    property Component headerDelegate: Rectangle {
        height: Math.round(56 * FlatStyle.scaleFactor)
        color: control.enabled ? FlatStyle.styleColor : FlatStyle.mediumFrameColor
        Text {
            anchors.fill: parent
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: styleData.textAlignment
            anchors.leftMargin: __columnMargin
            text: styleData.value
            elide: Text.ElideRight
            color: FlatStyle.selectedTextColor
            font {
                family: FlatStyle.fontFamily
                pixelSize: FlatStyle.defaultFontSize * FlatStyle.scaleFactor
            }

            renderType: FlatStyle.__renderType
        }

        Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: Math.round(36 * FlatStyle.scaleFactor)
            width: FlatStyle.onePixel
            color: "white"
            opacity: styleData.column !== control.columnCount - 1 ? 0.4 : 0
        }
    }

    property Component rowDelegate: Item {
        height: 46 * FlatStyle.scaleFactor
        readonly property color selectedColor: styleData.hasActiveFocus ?
                                               FlatStyle.focusedColor : FlatStyle.styleColor
        readonly property bool selected: control.enabled && (styleData.hasActiveFocus || styleData.selected)

        Rectangle {
            id: bg
            color: selected ? selectedColor :
                   styleData.alternate ? alternateBackgroundColor : backgroundColor
            opacity: !control.enabled ? (styleData.alternate ? __alternateBackgroundOpacity : 1.0) :
                     styleData.hasActiveFocus ? __focusedBackgroundOpacity :
                     styleData.selected ? (styleData.alternate ? __selectedBackgroundOpacity : __alternateBackgroundOpacity) :
                     (styleData.alternate ? __alternateBackgroundOpacity : 1.0)
            anchors.fill: parent
        }

        Rectangle {
            // Bottom separator
            visible: !control.alternatingRowColors
            color: selected ? selectedColor : FlatStyle.mediumFrameColor
            height: Math.round(1 * FlatStyle.scaleFactor)
            width: parent.width
            anchors.bottom: parent.bottom
        }

        Rectangle {
            // Top separator. Only visible if the current row is selected. It hides
            // the previous row's bottom separator when this row is selected or focused
            visible: selected && !control.alternatingRowColors
            color: selectedColor
            height: Math.round(1 * FlatStyle.scaleFactor)
            width: parent.width
            anchors.bottom: parent.top
        }
    }

    property Component itemDelegate: Item {
        height: Math.round(46 * FlatStyle.scaleFactor)
        implicitWidth: label.implicitWidth + __columnMargin

        Text {
            id: label
            text: styleData.value !== undefined ? styleData.value : ""
            elide: styleData.elideMode
            color: !control.enabled ? FlatStyle.disabledColor :
                   styleData.hasActiveFocus ? FlatStyle.focusedTextColor :
                   styleData.textColor
            opacity: !control.enabled ? FlatStyle.disabledOpacity : 1.0
            font {
                family: FlatStyle.fontFamily
                pixelSize: FlatStyle.defaultFontSize * FlatStyle.scaleFactor
                weight: control.enabled && (styleData.selected || styleData.hasActiveFocus) ?
                        Font.DemiBold : Font.Normal
            }
            renderType: FlatStyle.__renderType
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: __columnMargin
            anchors.verticalCenter: parent.verticalCenter
        }
    }

}
