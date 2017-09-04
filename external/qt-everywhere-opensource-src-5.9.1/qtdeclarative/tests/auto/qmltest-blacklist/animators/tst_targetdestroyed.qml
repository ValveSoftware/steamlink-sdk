/****************************************************************************
**
** Copyright (C) 2016 Gunnar Sletta <gunnar@sletta.org>
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

    TestCase {
        id: testcase
        name: "animators-targetdestroyed"
        when: false
        function test_endresult() {
            verify(true, "Got here :)");
        }
    }

    Rectangle {
        id: box
        width: 10
        height: 10
        color: "steelblue"
    }

    YAnimator {
        id: anim
        target: box
        from: 0;
        to: 100
        duration: 100
        loops: Animation.Infinite
        running: true
    }

    SequentialAnimation {
        running: true
        PauseAnimation { duration: 150 }
        ScriptAction { script: box.destroy(); }
        PauseAnimation { duration: 50 }
        ScriptAction { script: anim.destroy(); }
        PauseAnimation { duration: 50 }
        ScriptAction { script: testcase.when = true }
    }
}
