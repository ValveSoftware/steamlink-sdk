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

    TestCase {
        id: testCase
        name: "animators-nested"
        when: !animation.running
        function test_endresult() {
            compare(box.before, 2);
            compare(box.after, 2);
        }
    }

    Box {
        id: box

        anchors.centerIn: undefined

        property int before: 0;
        property int after: 0;

        SequentialAnimation {
            id: animation;
            ScriptAction { script: box.before++; }
            ParallelAnimation {
                ScaleAnimator { target: box; from: 2.0; to: 1; duration: 100; }
                OpacityAnimator { target: box; from: 0; to: 1; duration: 100; }
            }
            PauseAnimation { duration: 100 }
            SequentialAnimation {
                ParallelAnimation {
                    XAnimator { target: box; from: 0; to: 100; duration: 100 }
                    RotationAnimator { target: box; from: 0; to: 90; duration: 100 }
                }
                ParallelAnimation {
                    XAnimator { target: box; from: 100; to: 0; duration: 100 }
                    RotationAnimator { target: box; from: 90; to: 0; duration: 100 }
                }
            }
            ScriptAction { script: box.after++; }
            running: true
            loops: 2
        }
    }

}
