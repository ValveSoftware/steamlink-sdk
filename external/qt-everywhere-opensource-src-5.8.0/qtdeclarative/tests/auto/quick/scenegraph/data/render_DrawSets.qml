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
import SceneGraphTest 1.0

/*
   The purpose of the test is to verify that a batch of more than 64K
   vertices gets split into multiple drawsets and still rendered correctly.
   Both the clipped and unclipped batches have 50.000 rectangles resulting
   in 200.000 vertices in each batch, which should be plenty..

   #samples: 8

                PixelPos     R    G    B    Error-tolerance
   #base:         0   0     1.0  0.0  0.0        0.0
   #base:        99 199     1.0  0.0  0.0        0.0
   #base:       100   0     0.0  0.0  0.0        0.0
   #base:       199 199     0.0  0.0  0.0        0.0
   #final:        0   0     0.0  0.0  1.0        0.0
   #final:       99 199     0.0  0.0  1.0        0.0
   #final:      100   0     0.0  1.0  0.0        0.0
   #final:      199 199     0.0  1.0  0.0        0.0
*/

RenderTestBase
{
    id: root

    Column {
        id: clipped
        width: 100
        clip: true
        PerPixelRect { width: 100; height: 250; color: "red" }
        PerPixelRect { width: 100; height: 250; color: "blue" }
    }

    Column {
        id: unclipped
        x: 100
        width: 100
        PerPixelRect { width: 100; height: 250; color: "black" }
        PerPixelRect { width: 100; height: 250; color: "#00ff00" }
    }

    SequentialAnimation {
        id: animation
        NumberAnimation { target: clipped; property: "y"; from: 0; to: -clipped.height + root.height; duration: 100 }
        NumberAnimation { target: unclipped; property: "y"; from: 0; to: -unclipped.height + root.height; duration: 100 }
        PropertyAction { target: root; property: "finalStageComplete"; value: true; }
    }

    onEnterFinalStage: {
        animation.running = true;
    }
}
