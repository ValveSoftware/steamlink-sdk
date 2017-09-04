/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Virtual Keyboard module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

.pragma library

function renderSmoothedLine(ctx, trace, renderPos) {

    if (!trace)
        return renderPos

    if (renderPos >= trace.length)
        return renderPos

    // Fetch points and draw the initial "dot"
    var points, tp
    if (renderPos === 0) {
        points = trace.points()
        tp = points[renderPos++]
        ctx.beginPath()
        ctx.moveTo(tp.x, tp.y)
        ctx.lineTo(tp.x, tp.y + 0.000001)
        ctx.stroke()
    } else {
        points = trace.points(renderPos - 1)
    }

    // Draw smoothed line using quadratic curve
    var i = 1
    if (i + 1 < points.length) {
        var pt1, pt2
        if (renderPos === 1) {
            tp = points[i - 1]
        } else {
            pt1 = points[i - 1]
            pt2 = points[i]
            tp = Qt.point((pt1.x + pt2.x) / 2, (pt1.y + pt2.y) / 2)
        }
        ctx.beginPath()
        ctx.moveTo(tp.x, tp.y)
        while (i + 1 < points.length) {
            pt1 = points[i++]
            pt2 = points[i]
            tp = Qt.point((pt1.x + pt2.x) / 2, (pt1.y + pt2.y) / 2)
            ctx.quadraticCurveTo(pt1.x, pt1.y, tp.x, tp.y)
            ctx.moveTo(tp.x, tp.y)
        }
        ctx.stroke()
    }

    // Draw the remainder of the line
    if (trace.isFinal) {
        if (i < points.length) {
            tp = points[i - 1]
            ctx.beginPath()
            ctx.moveTo(tp.x, tp.y)
            tp = points[i++]
            ctx.lineTo(tp.x, tp.y)
            ctx.stroke()
        }
    }

    return renderPos + i - 1
}
