/****************************************************************************
**
** Copyright (C) 2016 Canonical Limited and/or its subsidiary(-ies).
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

import QtQuick 2.1
import Proxy 1.0

Item {

    ProxyTestInnerModel {
        id: innerModel
    }

    QSortFilterProxyModel {
        id: outerModel
        sourceModel: innerModel
        filterRegExp: RegExp("^H.*$")
    }

    width: 400
    height: 400
    ListView {
        anchors.fill: parent
        orientation: Qt.Vertical
        model: outerModel
        delegate: Rectangle {
            width: 400
            height: 50
            color: index % 2 ? "red" : "yellow"
        }
    }

    Timer {
        running: true
        interval: 500
        onTriggered: {
            innerModel.doStuff();
        }
    }
}
