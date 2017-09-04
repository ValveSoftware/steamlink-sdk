/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.9

Flickable {
    width: 200; height: 200
    contentWidth: rect.width; contentHeight: rect.height

    property real minContentY: 0
    property real maxContentY: 0
    onContentYChanged: {
        minContentY = Math.min(contentY, minContentY)
        maxContentY = Math.max(contentY, maxContentY)
    }

    property real minContentX: 0
    property real maxContentX: 0
    onContentXChanged: {
        minContentX = Math.min(contentX, minContentX)
        maxContentX = Math.max(contentX, maxContentX)
    }

    property real minVerticalOvershoot: 0
    property real maxVerticalOvershoot: 0
    onVerticalOvershootChanged: {
        minVerticalOvershoot = Math.min(verticalOvershoot, minVerticalOvershoot)
        maxVerticalOvershoot = Math.max(verticalOvershoot, maxVerticalOvershoot)
    }

    property real minHorizontalOvershoot: 0
    property real maxHorizontalOvershoot: 0
    onHorizontalOvershootChanged: {
        minHorizontalOvershoot = Math.min(horizontalOvershoot, minHorizontalOvershoot)
        maxHorizontalOvershoot = Math.max(horizontalOvershoot, maxHorizontalOvershoot)
    }

    function reset() {
        minContentY = contentY
        maxContentY = contentY
        minContentX = contentX
        maxContentX = contentX
        minVerticalOvershoot = 0
        maxVerticalOvershoot = 0
        minHorizontalOvershoot = 0
        maxHorizontalOvershoot = 0
    }

    Rectangle {
        id: rect
        color: "red"
        width: 400; height: 400
    }
}
