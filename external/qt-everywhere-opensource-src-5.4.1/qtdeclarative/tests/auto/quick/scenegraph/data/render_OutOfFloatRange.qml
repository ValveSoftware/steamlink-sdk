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
