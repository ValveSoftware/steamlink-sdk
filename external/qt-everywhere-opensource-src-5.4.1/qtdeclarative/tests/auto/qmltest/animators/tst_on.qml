/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
