/****************************************************************************
**
** Copyright (C) 2014 Canonical Limited and/or its subsidiary(-ies).
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

import QtQuick 2.3

ListView {
    id: list
    width: 400
    height: 600
    model: ListModel {
        ListElement { kind: "Bought" }
        ListElement { kind: "Available To Buy" }
    }

    delegate: GridView {
        id: grid
        objectName: "grid"
        height: Math.ceil(count / (width / cellWidth)) * cellHeight / 1
        width: list.width
        interactive: false
        property int count: 50
        cellHeight: 200
        cellWidth: cellHeight
        model: count
        property int createdItems: 0
        property int destroyedItems: 0

        delegate: Item {
            objectName: "delegate"
            width: cellWidth
            height: cellHeight
            Rectangle {
                width: parent.width - 20
                height: parent.height - 20
                anchors.centerIn: parent
                color: Math.random() * 2 > 1 ? "green" : "yellow";
                Text {
                    text: index
                }
                Component.onCompleted: createdItems++
                Component.onDestruction: destroyedItems++
            }
        }

        displayMarginBeginning: 0
        displayMarginEnd: -height

        function updatedDelegateCreationRange() {
            if (list.contentY + list.height <= grid.y) {
                // Not visible
                grid.displayMarginBeginning = 0
                grid.displayMarginEnd = -grid.height
            } else if (grid.y + grid.height <= list.contentY) {
                // Not visible
                grid.displayMarginBeginning = -grid.height
                grid.displayMarginEnd = 0
            } else {
                grid.displayMarginBeginning = -Math.max(list.contentY - grid.y, 0)
                grid.displayMarginEnd = -Math.max(grid.height - list.height - list.contentY + grid.y, 0)
            }
        }

        Component.onCompleted: updatedDelegateCreationRange();
        onHeightChanged: updatedDelegateCreationRange();
        Connections {
            target: list
            onContentYChanged: updatedDelegateCreationRange();
            onHeightChanged: updatedDelegateCreationRange();
        }
    }

    section.property: "kind"
    section.delegate: Text {
        height: 40
        font.pixelSize: 30
        text: section
    }
}
