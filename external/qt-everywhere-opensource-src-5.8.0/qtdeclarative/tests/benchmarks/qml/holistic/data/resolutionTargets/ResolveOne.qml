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

// This file has some nested items and does a lot of id resolution.
// This will allow us to benchmark the cost of resolving names in
// bindings.

import QtQuick 2.0

Item {
    id: root
    property int baseWidth: 500
    property int baseHeight: 600
    property string baseColor: "red"

    Item {
        id: childOne
        property int baseWidth: root.baseWidth - 1
        property int baseHeight: root.baseHeight - 1
        property string baseColor: root.baseColor

        Item {
            id: childTwo
            property int baseWidth: root.baseWidth - 2
            property int baseHeight: childOne.baseHeight - 1
            property string baseColor: childOne.baseColor

            Item {
                id: childThree
                property int baseWidth: childOne.baseWidth - 2
                property int baseHeight: root.baseHeight - 3
                property string baseColor: "blue"

                Item {
                    id: childFour
                    property int baseWidth: childTwo.baseWidth - 2
                    property int baseHeight: childThree.baseHeight - 1
                    property string baseColor: "earthy " + root.baseColor

                    Item {
                        id: childFive
                        property int baseWidth: root.baseWidth - 5
                        property int baseHeight: childFour.baseHeight - 1
                        property string baseColor: "carnelian " + childTwo.baseColor
                    }
                }

                Item {
                    id: childSix
                    property int baseWidth: parent.baseWidth - 3
                    property int baseHeight: root.baseHeight - 6
                    property string baseColor: "rust " + root.baseColor

                    Item {
                        id: childSeven
                        property int baseWidth: childOne.baseWidth - 6
                        property int baseHeight: childTwo.baseHeight - 5
                        property string baseColor: "sky " + childThree.baseColor
                    }
                }
            }
        }
    }

    Rectangle {
        width: childOne.baseWidth
        height: childOne.baseHeight
        color: parent.baseColor
        border.color: "black"
        border.width: 5
        radius: 10
    }
}
