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

import QtQuick 2.0
import QtTest 1.1

Rectangle {
    id:top
    width: 400
    height: 400
    color: "gray"

    Flickable {
        id: flick
        objectName: "flick"
        anchors.fill: parent
        contentWidth: 800
        contentHeight: 800

        Rectangle {
            width: flick.contentWidth
            height: flick.contentHeight
            color: "red"
            Rectangle {
                width: 50; height: 50; color: "blue"
                anchors.centerIn: parent
            }
        }
    }
    TestCase {
        name: "WheelEvents"
        when: windowShown       // Must have this line for events to work.

        function test_wheel() {
            //mouseWheel(item, x, y, xDelta, yDelta, buttons = Qt.NoButton, modifiers = Qt.NoModifier, delay = -1)
            mouseWheel(flick, 200, 200, 0, -120, Qt.NoButton, Qt.NoModifier, -1);
            wait(1000);
            verify(flick.contentY > 0);
            verify(flick.contentX == 0);
            flick.contentY = 0;
            verify(flick.contentY == 0);
            mouseWheel(flick, 200, 200, -120, 0, Qt.NoButton, Qt.NoModifier, -1);
            wait(1000);
            verify(flick.contentX > 0);
            verify(flick.contentY == 0);
        }

    }

}
