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

/*
    The test verifies that batching does not interfere with overlapping
    regions.

    #samples: 8
                 PixelPos     R    G    B    Error-tolerance
    #base:        10  10     1.0  0.0  0.0        0.0
    #base:        10 110     1.0  0.0  0.0        0.0
    #base:        10  11     0.0  0.0  1.0        0.0
    #base:        10 111     0.0  0.0  1.0        0.0

    #final:       10  10     1.0  0.0  0.0        0.05
    #final:       10 110     1.0  0.0  0.0        0.05
    #final:       10  11     0.0  0.0  1.0        0.05
    #final:       10 111     0.0  0.0  1.0        0.05
*/

RenderTestBase
{
    id: root
    property real offset: 0;
    property real farAway: 10000000;

    Item {
        y: -root.offset + 10
        x: 10
        Repeater {
            model: 200
            Rectangle {
                x: index % 100
                y: root.offset + (index < 100 ? 0 : 1);
                width: 1
                height: 1
                color: index < 100 ? "red" : "blue"
                antialiasing: true;
            }
        }
    }

    Item {
        y: -root.offset + 110
        x: 10
        Item {
            y: root.offset

            Repeater {
                model: 200
                Rectangle {
                    x: index % 100
                    y: (index < 100 ? 0 : 1);
                    width: 1
                    height: 1
                    color: index < 100 ? "red" : "blue"
                    antialiasing: true;
                }
            }
        }
    }

    onEnterFinalStage: {
        root.offset = root.farAway;
        root.finalStageComplete = true;
    }

}
