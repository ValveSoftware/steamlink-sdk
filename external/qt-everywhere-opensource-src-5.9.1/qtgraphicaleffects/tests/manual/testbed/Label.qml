/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Graphical Effects module.
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
    id: label
    property string caption: ""
    property string text: ""
    default property alias children: childrenItem.children
    anchors {left: parent.left; right: parent.right}
    height: Math.max(20, childrenItem.height)

    Text {
        id: captionText
        width: 110
        height: 20
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
        text: label.caption + ':'
        font.family: "Arial"
        font.pixelSize: 11
        color: "#B3B3B3"
    }

    Text {
        id: valueText
        anchors {left: captionText.right; right: parent.right; leftMargin: 11; verticalCenter: parent.verticalCenter}
        height: 20
        verticalAlignment: Text.AlignVCenter
        text: label.text
        font.family: "Arial"
        font.pixelSize: 11
        color: "#999999"
        visible: label.text.length > 0
    }

    Item {
        id: childrenItem
        anchors {left: captionText.right; leftMargin: 11; right: parent.right}
        height: childrenRect.height
    }
}
