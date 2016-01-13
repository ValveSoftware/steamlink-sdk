/****************************************************************************
**
** Copyright (C) 2014 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
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
import QtQuick.Window 2.0

Item {
    id: root;
    width: 200
    height: 200

    TestCase {
        id: testCase
        name: "animators-mixed"
        when: countdown == 0
        function test_endresult() {
            verify(true, "Just making sure we didn't crash");
        }
    }

    property int countdown: 5;

    Window {
        id: window

        width: 100
        height: 100

        ShaderEffect {
            width: 50
            height: 50

            property real t;
            UniformAnimator on t { from: 0; to: 1; duration: 1000; loops: Animation.Infinite }
            RotationAnimator on rotation { from: 0; to: 360; duration: 1000; loops: Animation.Infinite }
            ScaleAnimator on scale { from: 0.5; to: 1.5; duration: 1000; loops: Animation.Infinite }
            XAnimator on x { from: 0; to: 50; duration: 1000; loops: Animation.Infinite }
            YAnimator on y { from: 0; to: 50; duration: 1000; loops: Animation.Infinite }
            OpacityAnimator on opacity { from: 1; to: 0.5; duration: 1000; loops: Animation.Infinite }

            fragmentShader: "
                uniform lowp float t;
                uniform lowp float qt_Opacity;
                varying highp vec2 qt_TexCoord0;
                void main() {
                    gl_FragColor = vec4(qt_TexCoord0, t, 1) * qt_Opacity;
                }
                "
        }

        visible: true
    }

    Timer {
        interval: 250
        running: true
        repeat: true
        onTriggered: {
            if (window.visible)
                --countdown
            window.visible = !window.visible;
        }
    }
}
