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
    The test verifies that batches of translucent items get split
    into multiple batches when an item in them change opacity.

                 PixelPos     R    G    B    Error-tolerance
    #base:         0   0     1.0  0.5  0.5        0.05
    #base:       100   0     0.5  0.5  1.0        0.05
    #base:         0 100     1.0  0.5  0.5        0.05
    #base:       100 100     0.5  0.5  1.0        0.05
    #final:        0   0     1.0  0.5  0.5        0.05
    #final:      100   0     0.1  0.1  1.0        0.05
    #final:        0 100     1.0  0.5  0.5        0.05
    #final:      100 100     0.9  0.9  1.0        0.05

    #samples: 8
*/

RenderTestBase {

    Item {
        Rectangle { id: redUnclipped;  opacity: 0.5; width: 50; height: 50; color: "red" }
        Rectangle { id: blueUnclipped; opacity: 0.5; width: 50; height: 50; color: "blue"; x: 100 }
    }

    Item {
        clip: true;
        y: 100
        width: 200
        height: 100
        Rectangle { id: redClipped;  opacity: 0.5; width: 50; height: 50; color: "red" }
        Rectangle { id: blueClipped; opacity: 0.5; width: 50; height: 50; color: "blue"; x: 100 }
    }

    onEnterFinalStage: {
        blueUnclipped.opacity = 0.9
        blueClipped.opacity = 0.1
        finalStageComplete = true;
    }

}
