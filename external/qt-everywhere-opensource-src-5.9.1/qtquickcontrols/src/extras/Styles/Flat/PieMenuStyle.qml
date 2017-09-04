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

import QtQuick 2.3
import QtQuick.Controls.Styles 1.4 as Base
import QtQuick.Controls.Styles.Flat 1.0
import QtQuick.Extras.Private 1.0

Base.PieMenuStyle {
    id: pieMenuStyle
    shadowRadius: 0
    shadowSpread: 0
    shadowColor: "transparent"
    backgroundColor: FlatStyle.styleColor
    selectionColor: FlatStyle.styleColor
    startAngle: -78
    endAngle: 78
    cancelRadius: radius * 0.4352

    __implicitWidth: Math.round(85 * 2 * FlatStyle.scaleFactor)
    __implicitHeight: __implicitWidth

    title: null

    menuItem: Item {
        readonly property var __styleData: styleData

        Canvas {
            id: actionCanvas
            antialiasing: true
            anchors.fill: parent
            anchors.margins: -1

            Connections {
                target: pieMenuStyle
                onStartAngleChanged: actionCanvas.requestPaint()
                onEndAngleChanged: actionCanvas.requestPaint()
            }

            Connections {
                target: __styleData
                onPressedChanged: actionCanvas.requestPaint()
            }

            readonly property bool stroke: !__styleData.hovered
            readonly property real spacingThickness: Math.max(4 * FlatStyle.scaleFactor, radius * 0.05)
            // It's actually one, but stroking always goes 50/50 on the inside/outside, so this is
            // an easy way of making it one pixel without having to mess around with more drawing code.
            // This will work as long as the fill and stroke colors are both completely opaque.
            readonly property real lineWidth: FlatStyle.twoPixels
            // The following properties are all for either side of a radius/circumference, so the total is actually double.
            readonly property real outerMenuItemSpacing: Math.asin(spacingThickness / (2 * radius))
            readonly property real innerMenuItemSpacing: Math.asin(spacingThickness / (2 * cancelRadius))
            // The total angle to subtract from the circumference of the inner radius arc
            readonly property real innerRadiusStrokeCircumferenceAdjustment: Math.asin(lineWidth / (2 * cancelRadius))
            readonly property real outerRadiusStrokeCircumferenceAdjustment: Math.asin(lineWidth / (2 * radius))

            onStrokeChanged: actionCanvas.requestPaint()

            function drawRingSection(ctx, x, y, section, r, ringWidth, ringColor) {
                if (stroke) {
                    ctx.strokeStyle = ringColor;
                    ctx.fillStyle = "white";
                } else {
                    ctx.fillStyle = ringColor;
                }

                ctx.beginPath();

                if (stroke)
                    ctx.lineWidth = lineWidth;
                var start = control.__protectedScope.sectionStartAngle(section);
                var end = control.__protectedScope.sectionEndAngle(section);
                // Adjust the radius to account for stroke being 50% inside/50% outside.
                var radius = !stroke ? r : r - ctx.lineWidth / 2;
                // Add spacing between the items, while still accounting for reversed menus.
                if (start > end) {
                    start -= outerMenuItemSpacing;
                    end += outerMenuItemSpacing;
                    if (stroke) {
                        start -= outerRadiusStrokeCircumferenceAdjustment;
                        end += outerRadiusStrokeCircumferenceAdjustment;
                    }
                } else {
                    start += outerMenuItemSpacing;
                    end -= outerMenuItemSpacing;
                    if (stroke) {
                        start += outerRadiusStrokeCircumferenceAdjustment;
                        end -= outerRadiusStrokeCircumferenceAdjustment;
                    }
                }

                ctx.arc(x, y, radius, start, end, start > end);

                start = control.__protectedScope.sectionStartAngle(section);
                end = control.__protectedScope.sectionEndAngle(section);
                radius = !stroke ? ringWidth : ringWidth + ctx.lineWidth / 2;
                if (start > end) {
                    start -= innerMenuItemSpacing;
                    end += innerMenuItemSpacing;
                    if (stroke) {
                        start -= innerRadiusStrokeCircumferenceAdjustment;
                        end += innerRadiusStrokeCircumferenceAdjustment;
                    }
                } else {
                    start += innerMenuItemSpacing;
                    end -= innerMenuItemSpacing;
                    if (stroke) {
                        start += innerRadiusStrokeCircumferenceAdjustment;
                        end -= innerRadiusStrokeCircumferenceAdjustment;
                    }
                }

                ctx.arc(x, y, radius, end, start, start <= end);
                ctx.closePath();

                if (stroke)
                    ctx.stroke();

                ctx.fill();
            }

            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                drawRingSection(ctx, width / 2, height / 2, styleData.index, radius, cancelRadius,
                    __styleData.pressed ? FlatStyle.pressedColorAlt : (stroke ? backgroundColor : selectionColor));
            }
        }

        PieMenuIcon {
            control: pieMenuStyle.control
            styleData: __styleData
        }
    }
}
