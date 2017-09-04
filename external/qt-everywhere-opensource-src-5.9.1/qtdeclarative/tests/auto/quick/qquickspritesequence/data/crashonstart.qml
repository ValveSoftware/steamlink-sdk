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

//QTBUG-24797
import QtQuick 2.0

Rectangle {
    width: 800
    height: 800

    SpriteSequence {
        id: mysprite
        sprites: [s1,s2]
        scale: 2
        height: 200
        width: 200
        anchors.centerIn: parent
    }

    Component.onCompleted: mysprite.jumpTo("running")
    Sprite {
        id: s1
        name: "standing"
        frameCount: 12
        frameDuration: 80
        source: "squarefacesprite.png"
    }

    Sprite {
        id: s2
        name: "running"
        frameCount: 6
        frameDuration: 80
        source: "squarefacesprite.png"
    }
}
