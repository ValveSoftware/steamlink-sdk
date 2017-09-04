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
        id: testcase
        name: "animators-stopped"
        when: false
        function test_endresult() {
            verify(true);
        }
    }

    ShaderEffect {
        id: shaderEffect
        property real t;
        width: 10
        height: 10

        fragmentShader: "
            highp uniform float t;
            void main() {
                gl_FragColor = vec4(t, t, t, 1.0);
            }
        "
        UniformAnimator { id: uniformAnimator; target: shaderEffect; uniform: "t"; loops: Animation.Infinite; running: true; }
    }

    Box {
        id: box

        ScaleAnimator {    id: scaleAnimator;    target: box; loops: Animation.Infinite; running: true; }
        XAnimator {        id: xAnimator;        target: box; loops: Animation.Infinite; running: true; }
        YAnimator {        id: yAnimator;        target: box; loops: Animation.Infinite; running: true; }
        RotationAnimator { id: rotationAnimator; target: box; loops: Animation.Infinite; running: true; }
        OpacityAnimator {  id: opacityAnimator;  target: box; loops: Animation.Infinite; running: true; }

        Timer {
            id: timer;
            interval: 500
            running: true
            repeat: false
            onTriggered: {
                xAnimator.stop();
                yAnimator.stop();
                scaleAnimator.stop()
                rotationAnimator.stop();
                rotationAnimator.stop();
                uniformAnimator.stop();
                testcase.when = true;
            }
        }
    }
}
