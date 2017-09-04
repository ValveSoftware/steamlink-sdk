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
    This test verifies that when we have an update to opacity above
    a batch root, the opacity of the batch root's children is rendered
    correctly. The Text element has 1000 glyphs in it, which is needed
    for contentRoot to become a batch root when the scale changes.

    #samples: 2
                 PixelPos     R    G    B    Error-tolerance
    #base:        50  50     0.0  0.0  1.0       0.0
    #final:       50  50     0.5  0.5  1.0       0.05
*/

RenderTestBase {
    id: root

    Item {
        id: failRoot;
        property alias itemScale: contentItem.scale

        Item {
            id: contentItem
            width: 100
            height: 100
            Rectangle {
                width: 100
                height: 100
                color: "blue"
                Text {
                    id: input
                    color: "black"
                    Component.onCompleted: { for (var i = 0; i<1000; ++i) input.text += 'x' }
                }
            }
        }
    }

    SequentialAnimation {
        id: unifiedAnimation;
        NumberAnimation { properties: "opacity,itemScale"; duration: 256; from: 1; to: 0.5; target: failRoot }
        ScriptAction { script: root.finalStageComplete = true; }
    }

    onEnterFinalStage: {
        unifiedAnimation.running = true;
    }

}
