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
import QtQuick.Controls.Styles 1.4 as Base
import QtQuick.Controls.Styles.Flat 1.0
import QtQuick.Extras.Private 1.0
import QtQuick.Extras.Private.CppUtils 1.1

Base.CircularGaugeStyle {
    id: circularGaugeStyle

    tickmarkInset: __thickArcTickmarklessMargin + __thickArcLineWidth / 2 - __tickmarkLength / 2
    minorTickmarkInset: __thickArcTickmarklessMargin + __thickArcLineWidth / 2 - (__tickmarkLength / 2) / 2
    labelInset: Math.max(textMetrics.width, textMetrics.height) / 2

    // A stroked arc starts losing its shape when it is very thick, so prevent that from happening.
    readonly property real __thickArcLineWidth: Math.min(22 * FlatStyle.scaleFactor, Math.round(outerRadius * 0.2))
    readonly property real __thickArcTickmarklessMargin: labelInset * 2
    readonly property real __tickmarkLength: __thickArcLineWidth * 0.5
    readonly property real __tickmarkWidth: FlatStyle.onePixel
    readonly property real __fontSize: Math.max(6, __protectedScope.toPixels(0.12))
    // TODO: add this to Utils or something; it's not specific to this control
    readonly property real __antialiasingAdjustment: FlatStyle.onePixel

    Text {
        id: textMetrics
        font.pixelSize: __fontSize
        text: control.maximumValue.toFixed(0)
        visible: false
    }

    background: Item {
        implicitWidth: Math.round(160 * FlatStyle.scaleFactor)
        implicitHeight: Math.round(160 * FlatStyle.scaleFactor)

        readonly property real thinArcLineWidth: FlatStyle.onePixel
        readonly property bool tickmarklessVariant: false
        readonly property real arcDistance: thinArcLineWidth * 3

        Connections {
            target: control
            onValueChanged: thickArc.requestPaint()
            onMinimumValueChanged: thickArc.requestPaint()
            onMaximumValueChanged: thickArc.requestPaint()
            onEnabledChanged: {
                thickArc.requestPaint();
                thinArc.requestPaint();
            }
        }

        // TODO: test performance of this on the Pi
        Canvas {
            id: thickArc
            anchors.fill: parent
            anchors.margins: Math.floor(tickmarklessVariant ? arcDistance : __thickArcTickmarklessMargin)

            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();

                ctx.lineWidth = __thickArcLineWidth;
                var radius = Math.floor(width / 2 - ctx.lineWidth / 2 - __antialiasingAdjustment);
                if (radius < 0)
                    return;

                ctx.beginPath();
                ctx.globalAlpha = control.enabled ? 1 : 0.2;
                ctx.strokeStyle = control.enabled ? FlatStyle.styleColor : FlatStyle.disabledColor;
                var startAngle = MathUtils.degToRad(minimumValueAngle - 90);
                var endAngle = MathUtils.degToRad(needleRotation - 90);
                ctx.arc(Math.floor(width / 2), Math.floor(height / 2), radius, startAngle, endAngle, false);
                ctx.stroke();
            }
        }

        Canvas {
            id: thinArc
            anchors.fill: parent
            anchors.margins: Math.floor(tickmarklessVariant ? 0 : thickArc.anchors.margins + __thickArcLineWidth + arcDistance)

            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();

                ctx.lineWidth = thinArcLineWidth;
                var radius = width / 2 - ctx.lineWidth / 2 - __antialiasingAdjustment;
                if (radius < 0)
                    return;

                ctx.beginPath();
                ctx.globalAlpha = control.enabled ? 1 : 0.2;
                ctx.strokeStyle = control.enabled ? FlatStyle.styleColor : FlatStyle.disabledColor;
                var startAngle = MathUtils.degToRad(minimumValueAngle - 90);
                var endAngle = MathUtils.degToRad(maximumValueAngle - 90);
                ctx.arc(Math.floor(width / 2), Math.floor(height / 2), radius, startAngle, endAngle, false);
                ctx.stroke();
            }
        }
    }

    needle: null
    foreground: null

    tickmark: Rectangle {
        implicitWidth: __tickmarkWidth
        implicitHeight: __tickmarkLength
        antialiasing: true
        color: control.value < styleData.value || isEdgeTickmark ? (control.enabled ? FlatStyle.styleColor : FlatStyle.disabledColor) : FlatStyle.selectedTextColor
        opacity: control.value < styleData.value || isEdgeTickmark ? (control.enabled ? 1 : 0.2) : 0.5
        visible: (!isEdgeTickmark) || (control.value < styleData.value)
            || (styleData.value === control.minimumValue && control.value === control.minimumValue)

        readonly property bool isEdgeTickmark: styleData.value === control.minimumValue || styleData.value === control.maximumValue
    }

    minorTickmark: Rectangle {
        implicitWidth: __tickmarkWidth
        implicitHeight: __tickmarkLength / 2
        antialiasing: true
        color: control.enabled ? FlatStyle.styleColor : FlatStyle.disabledColor
        visible: control.value < styleData.value
        opacity: control.enabled ? 1 : 0.2
    }

    tickmarkLabel: Text {
        font.pixelSize: __fontSize
        text: styleData.value
        color: control.enabled ? FlatStyle.defaultTextColor : FlatStyle.disabledColor
        opacity: control.enabled ? 1 : FlatStyle.disabledOpacity
        antialiasing: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
