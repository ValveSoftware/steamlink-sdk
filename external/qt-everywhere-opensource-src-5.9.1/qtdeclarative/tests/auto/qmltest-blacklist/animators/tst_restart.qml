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

import QtQuick 2.2
import QtTest 1.1

Item {
    id: root;
    width: 200
    height: 200

    property int restartCount: 5;

    TestCase {
        id: testcase
        name: "animators-restart"
        when: root.restartCount == 0 && animation.running == false;
        function test_endresult() {
            compare(box.scale, 2);
        }
    }

    Box {
        id: box

        ScaleAnimator {
            id: animation
            target: box;
            from: 1;
            to: 2.0;
            duration: 100;
            loops: 1
            running: false;
        }

        Timer {
            id: timer;
            interval: 500
            running: true
            repeat: true
            onTriggered: {
                animation.running = true;
                --root.restartCount;
            }
        }
    }
}
