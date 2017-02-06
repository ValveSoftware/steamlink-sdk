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

import QtQuick 2.0

Item {
    id: root
    property int dynamicWidth: 100
    property int dynamicHeight: rect1.height + rect2.height
    property int widthSignaledProperty: 10
    property int heightSignaledProperty: 10

    Rectangle {
        id: rect1
        width: root.dynamicWidth + 20
        height: width + (5*3) - 8 + (width/9)
        color: "red"
        border.color: "black"
        border.width: 5
        radius: 10
    }

    Rectangle {
        id: rect2
        width: rect1.width - 50
        height: width + (5*4) - 6 + (width/3)
        color: "red"
        border.color: "black"
        border.width: 5
        radius: 10
    }

    onDynamicWidthChanged: {
        widthSignaledProperty = widthSignaledProperty + (6*5) - 2;
    }

    onDynamicHeightChanged: {
        heightSignaledProperty = widthSignaledProperty + heightSignaledProperty + (5*3) - 7;
    }
}
