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
import QtQuick.Controls.Styles.Flat 1.0
import QtQuick.Extras.Private 1.0

Base.GaugeStyle {
    id: gaugeStyle

    readonly property int __totalValueBarWidth: Math.round(22 * FlatStyle.scaleFactor + __lineSpacing + __lineWidth)
    readonly property int __actualValueBarWidth: __totalValueBarWidth - __lineSpacing - __lineWidth
    readonly property int __lineWidth: FlatStyle.onePixel
    readonly property int __lineSpacing: Math.round(3 * FlatStyle.scaleFactor)

    background: Item {
        Rectangle {
            color: control.enabled ? FlatStyle.styleColor : FlatStyle.disabledColor
            opacity: control.enabled ? 1 : 0.2
            width: __lineWidth
            height: parent.height
            x: control.tickmarkAlignment === Qt.AlignLeft || control.tickmarkAlignment === Qt.AlignTop ? parent.width - width : 0
        }
    }

    valueBar: Item {
        implicitWidth: __totalValueBarWidth

        Rectangle {
            color: control.enabled ? FlatStyle.styleColor : FlatStyle.disabledColor
            opacity: control.enabled ? 1 : 0.2
            x: control.tickmarkAlignment === Qt.AlignRight || control.tickmarkAlignment === Qt.AlignBottom ? __lineWidth + __lineSpacing : 0
            implicitWidth: __actualValueBarWidth
            height: parent.height
        }
    }

    foreground: null

    tickmark: Item {
        id: tickmarkItem
        implicitWidth: Math.round(12 * FlatStyle.scaleFactor)
        implicitHeight: FlatStyle.onePixel

        Rectangle {
            x: control.tickmarkAlignment === Qt.AlignLeft || control.tickmarkAlignment === Qt.AlignTop
               ? parent.width + __actualValueBarWidth / 2 - width / 2
               : -__actualValueBarWidth / 2 - width / 2
            // Length of the tickmark is the same as the spacing between the tickmarks and labels.
            width: parent.width
            height: parent.height
            color: gaugeStyle.valuePosition > styleData.valuePosition + height ? FlatStyle.selectedTextColor : (control.enabled ? FlatStyle.styleColor : FlatStyle.disabledColor)
            opacity: gaugeStyle.valuePosition > styleData.valuePosition + height ? 0.5 : (control.enabled ? 1 : 0.2)
            visible: (styleData.index === 0 && gaugeStyle.valuePosition === 0) ||
                (styleData.index !== 0 && (gaugeStyle.valuePosition <= styleData.valuePosition || gaugeStyle.valuePosition > styleData.valuePosition + height))
        }
    }

    minorTickmark: Item {
        implicitWidth: Math.round(6 * FlatStyle.scaleFactor)
        implicitHeight: FlatStyle.onePixel

        Rectangle {
            x: control.tickmarkAlignment === Qt.AlignLeft || control.tickmarkAlignment === Qt.AlignTop
               ? parent.width + __actualValueBarWidth / 2 - width / 2
               : -__actualValueBarWidth / 2 - width / 2
            width: parent.width
            height: parent.height
            color: control.enabled ? FlatStyle.styleColor : FlatStyle.disabledColor
            opacity: control.enabled ? 1 : 0.2
            visible: gaugeStyle.valuePosition <= styleData.valuePosition
        }
    }

    tickmarkLabel: Item {
        implicitWidth: textLabel.implicitWidth
        implicitHeight: textLabel.implicitHeight

        Label {
            id: textLabel
            text: control.formatValue(styleData.value)
            font: control.font
            color: control.enabled ? FlatStyle.defaultTextColor : FlatStyle.disabledColor
            opacity: control.enabled ? 1 : FlatStyle.disabledOpacity
            renderType: FlatStyle.__renderType
            Connections {
                target: control
                // Setting an anchor to undefined leaves it in the position it was in last.
                // We don't want that, as we want the label's y pos to be at zero when it's not anchored.
                // Using a binding here whose when property is true when control.orientation === Qt.Horizontal
                // doesn't work.
                onOrientationChanged: textLabel.y = 0
            }
            anchors.baseline: control.orientation === Qt.Vertical ? parent.verticalCenter : undefined
        }
    }
}
