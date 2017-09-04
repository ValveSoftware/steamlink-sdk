/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd.
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

import QtQuick 2.3

Item {
    width: 400; height: 400

    ListView {
        id: view
        anchors.top: header.bottom
        anchors.bottom: footer.top
        width: parent.width

        cacheBuffer: 0
        displayMarginBeginning: 60
        displayMarginEnd: 60

        model: 100
        delegate: Rectangle {
            objectName: "delegate"
            width: parent.width
            height: 25
            color: index % 2 ? "steelblue" : "lightsteelblue"
            Text {
                anchors.centerIn: parent
                text: index
            }
        }
    }

    Rectangle {
        id: header
        width: parent.width; height: 60
        color: "#80FF0000"
    }

    Rectangle {
        id: footer
        anchors.bottom: parent.bottom
        width: parent.width; height: 60
        color: "#80FF0000"
    }
}
