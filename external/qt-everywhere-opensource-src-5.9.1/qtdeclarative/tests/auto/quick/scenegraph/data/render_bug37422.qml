/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
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

/*
    The test verifies that batching does not interfere with overlapping
    regions.

    #samples: 8
                 PixelPos     R    G    B    Error-tolerance
    #base:         0   0     1.0  0.0  0.0        0.05
    #base:         1   1     0.0  0.0  1.0        0.05
    #base:        10  10     1.0  0.0  0.0        0.05
    #base:         1  11     0.0  0.0  1.0        0.05

    #final:        0   0     1.0  0.0  0.0        0.05
    #final:        1   1     0.0  1.0  0.0        0.05
    #final:       10  10     1.0  0.0  0.0        0.05
    #final:        1  11     0.0  1.0  0.0        0.05

*/

RenderTestBase
{
    id: root

    opacity: 0.99

    Rectangle {
        width: 100
        height: 9
        color: Qt.rgba(1, 0, 0);

        Rectangle {
            id: box
            width: 5
            height: 5
            x: 1
            y: 1
            color: Qt.rgba(0, 0, 1);
        }
    }

    ShaderEffect { // Something which blends and is different than rectangle. Will get its own batch
        width: 100
        height: 9
        y: 10
        fragmentShader: "varying highp vec2 qt_TexCoord0; void main() { gl_FragColor = vec4(1, 0, 0, 1); }"

        Rectangle {
            width: 5
            height: 5
            x: 1
            y: 1
            color: box.color
        }
    }

    onEnterFinalStage: {
        box.color = Qt.rgba(0, 1, 0);
        root.finalStageComplete = true;
    }

}
