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
    id: root; objectName: "root"
    width: 200; height: 200

    Item { id: itemA; objectName: "itemA"; x: 50; y: 50; width: 25; height: 70 }

    Item {
        x: 50; y: 50
        rotation: 45
        Item { id: itemB; objectName: "itemB"; x: 100; y: 100; width: 30; height: 45 }
    }

    function mapAToB(x, y, w, h) {
        var pos = itemA.mapToItem(itemB, x, y, w, h)
        return Qt.rect(pos.x, pos.y, pos.width, pos.height)
    }

    function mapAFromB(x, y, w, h) {
        var pos = itemA.mapFromItem(itemB, x, y, w, h)
        return Qt.rect(pos.x, pos.y, pos.width, pos.height)
    }

    function mapAToNull(x, y, w, h) {
        var pos = itemA.mapToItem(null, x, y, w, h)
        return Qt.rect(pos.x, pos.y, pos.width, pos.height)
    }

    function mapAFromNull(x, y, w, h) {
        var pos = itemA.mapFromItem(null, x, y, w, h)
        return Qt.rect(pos.x, pos.y, pos.width, pos.height)
    }

    function checkMapAToInvalid(x, y, w, h) {
        try {
            itemA.mapToItem(1122, x, y, w, h)
        } catch (e) {
            return e instanceof TypeError
        }
        return false;
    }

    function checkMapAFromInvalid(x, y, w, h) {
        try {
            itemA.mapFromItem(1122, x, y, w, h)
        } catch (e) {
            return e instanceof TypeError
        }
        return false;
    }
}
