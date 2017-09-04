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
        name: "animators-on"
        when: !animx.running && !animy.running
              && !anims.running && !animr.running
              && !animo.running;
        function test_endresult() {
            tryCompare(box, 'xChangeCounter', 1);
            compare(box.yChangeCounter, 1);
            compare(box.scaleChangeCounter, 1);
            compare(box.rotationChangeCounter, 1);
            compare(box.opacityChangeCounter, 1);
            compare(box.x, 100);
            compare(box.y, 100);
            compare(box.scale, 2);
            compare(box.rotation, 180);
            compare(box.opacity, 0.5);
        }
    }

    Box {
        id: box
        anchors.centerIn: undefined
        XAnimator on x { id: animx; from: 0; to: 100; duration: 100 }
        YAnimator on y { id: animy; from: 0; to: 100; duration: 100 }
        ScaleAnimator on scale { id: anims; from: 1; to: 2; duration: 100 }
        RotationAnimator on rotation { id: animr ; from: 0; to: 180; duration: 100 }
        OpacityAnimator on opacity { id: animo; from: 1; to: 0.5; duration: 100 }
    }
}
