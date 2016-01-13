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
