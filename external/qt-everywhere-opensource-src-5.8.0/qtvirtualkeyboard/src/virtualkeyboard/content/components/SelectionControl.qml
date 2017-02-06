/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Virtual Keyboard module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.1

Item {
    id: root
    property bool handleIsMoving: false
    visible: InputContext.selectionControlVisible || handleIsMoving

    Loader {
        id: anchorHandle
        sourceComponent: keyboard.style.selectionHandle
        x: visible ? Qt.inputMethod.anchorRectangle.x - width/2 : 0
        y: visible ? Qt.inputMethod.anchorRectangle.y + Qt.inputMethod.anchorRectangle.height : 0

        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }
        opacity: InputContext.anchorRectIntersectsClipRect ? 1.0 : 0.0

        MouseArea {
            width: parent.width * 2
            height: width * 1.12
            anchors.centerIn: parent
            onPositionChanged: {
                // we don't move the handles, the handles will move as the selection changes.
                // The middle of a handle is mapped to the middle of the line above it
                root.handleIsMoving = true
                var xx = x + anchorHandle.x + mouse.x
                var yy = y + anchorHandle.y + mouse.y - (anchorHandle.height + Qt.inputMethod.anchorRectangle.height)/2
                var x2 = cursorHandle.x + cursorHandle.width/2
                var y2 = cursorHandle.y - Qt.inputMethod.cursorRectangle.height/2
                InputContext.setSelectionOnFocusObject(Qt.point(xx,yy), Qt.point(x2,y2))
            }
            onReleased: {
                root.handleIsMoving = false
            }
        }
    }

    // selection cursor handle
    Loader {
        id: cursorHandle
        sourceComponent: keyboard.style.selectionHandle
        x: visible ? Qt.inputMethod.cursorRectangle.x - width/2 : 0
        y: visible ? Qt.inputMethod.cursorRectangle.y + Qt.inputMethod.cursorRectangle.height : 0

        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }
        opacity: InputContext.cursorRectIntersectsClipRect ? 1.0 : 0.0

        MouseArea {
            width: parent.width * 2
            height: width * 1.12
            anchors.centerIn: parent
            onPositionChanged: {
                // we don't move the handles, the handles will move as the selection changes.
                root.handleIsMoving = true
                var xx = anchorHandle.x + anchorHandle.width/2
                var yy = anchorHandle.y - Qt.inputMethod.anchorRectangle.height/2
                var x2 = x + cursorHandle.x + mouse.x
                var y2 = y + cursorHandle.y + mouse.y - (cursorHandle.height + Qt.inputMethod.cursorRectangle.height)/2
                InputContext.setSelectionOnFocusObject(Qt.point(xx, yy), Qt.point(x2, y2))
            }
            onReleased: {
                root.handleIsMoving = false
            }
        }
    }
}
