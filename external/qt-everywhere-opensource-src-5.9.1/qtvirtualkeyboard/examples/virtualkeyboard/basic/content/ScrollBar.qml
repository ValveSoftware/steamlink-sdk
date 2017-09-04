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

import QtQuick 2.0

Item {
    property var scrollArea: parent

    width: 6
    opacity: scrollArea && scrollArea.movingVertically ? 1.0 : 0.0
    visible: scrollArea && scrollArea.contentHeight >= scrollArea.height
    anchors { right: parent.right; top: parent.top; bottom: parent.bottom; margins: 2 }

    Behavior on opacity { NumberAnimation { properties: "opacity"; duration: 600 } }

    function size() {
        var nh = scrollArea.visibleArea.heightRatio * height
        var ny = scrollArea.visibleArea.yPosition * height
        return ny > 3 ? Math.min(nh, Math.ceil(height - 3 - ny)) : nh + ny
    }
    Rectangle {
        x: 1
        y: scrollArea ? Math.max(2, scrollArea.visibleArea.yPosition * parent.height) : 0
        height: scrollArea ? size() : 0
        width: parent.width - 2
        color: Qt.rgba(1.0, 1.0, 1.0, 0.5)
        radius: 1
    }
}
