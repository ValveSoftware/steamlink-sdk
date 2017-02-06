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

.import "unipen_data.js" as UnipenData

function emulate(testcase, hwrInputArea, ch, instant) {
    var chKey = (((typeof ch == "number") ? ch : ch.charCodeAt(0)) + 0x100000000).toString(16).substr(1)
    while (chKey.length > 4 && chKey[0] === '0')
        chKey = chKey.substring(1)
    chKey = "0x" + chKey
    if (!UnipenData.unipenData.hasOwnProperty(chKey))
        return false
    var chData = UnipenData.unipenData[chKey]
    var scale = Math.min(hwrInputArea.width / chData[".X_DIM"], hwrInputArea.height / chData[".Y_DIM"])
    var strokes = UnipenData.unipenData[chKey][".PEN"]
    var boundingBox = calculateBoundingBox(strokes)
    var boxOffset = Qt.point(-boundingBox.x * scale + (hwrInputArea.width - boundingBox.width * scale) / 2, -boundingBox.y * scale + (hwrInputArea.height - boundingBox.height * scale) / 2)
    var t = 0
    for (var strokeIndex = 0; strokeIndex < strokes.length; strokeIndex++) {
        var stroke = strokes[strokeIndex]
        for (var i = 0; i < stroke.length; i++) {
            var strokeData = stroke[i]
            var pt = Qt.point(strokeData[0] * scale + boxOffset.x, strokeData[1] * scale + boxOffset.y)
            if (instant)
                t = strokeData[2]
            if (i == 0) {
                t = strokeData[2]
                testcase.mousePress(hwrInputArea, pt.x, pt.y, Qt.LeftButton, 0, strokeData[2] - t)
            } else {
                testcase.mouseMove(hwrInputArea, pt.x, pt.y, strokeData[2] - t, Qt.LeftButton)
            }
            if (i + 1 === stroke.length)
                testcase.mouseRelease(hwrInputArea, pt.x, pt.y, Qt.LeftButton, 0, instant ? 1 : strokeData[2] - t)
            t = strokeData[2]
        }
    }
    return true
}

function calculateBoundingBox(unipenStrokes) {
    var bboxLeft = 2147483647
    var bboxRight = -2147483647
    var bboxTop = 2147483647
    var bboxBottom = -2147483647
    for (var strokeIndex = 0; strokeIndex < unipenStrokes.length; strokeIndex++) {
        var stroke = unipenStrokes[strokeIndex]
        for (var i = 0; i < stroke.length; i++) {
            var strokeData = stroke[i]
            var x = strokeData[0]
            if (bboxLeft > x)
                bboxLeft = x
            if (bboxRight < x)
                bboxRight = x
            var y = strokeData[1]
            if (bboxTop > y)
                bboxTop = y
            if (bboxBottom < y)
                bboxBottom = y
        }
    }
    if (bboxLeft > bboxRight || bboxTop > bboxBottom)
        return Qt.rect(0, 0, 0, 0)
    return Qt.rect(bboxLeft, bboxTop, bboxRight - bboxLeft, bboxBottom -bboxTop)
}
