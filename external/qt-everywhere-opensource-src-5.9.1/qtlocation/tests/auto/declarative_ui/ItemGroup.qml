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

import QtQuick 2.4
import QtPositioning 5.6
import QtLocation 5.9
import QtLocation.Test 5.6

MapItemGroup {
    id: itemGroup
    property double latitude : (mainRectangle.topLeft.latitude + mainRectangle.bottomRight.latitude) / 2.0
    property double longitude: (mainRectangle.topLeft.longitude + mainRectangle.bottomRight.longitude) / 2.0
    property double radius: 100 * 1000

    MapRectangle {
        id: mainRectangle
        topLeft: QtPositioning.coordinate(43, -3)
        bottomRight: QtPositioning.coordinate(37, 3)
        opacity: 0.05
        visible: true
        color: 'blue'
    }

    MapCircle {
        id: groupCircle
        center: QtPositioning.coordinate(parent.latitude, parent.longitude)
        radius: parent.radius
        color: 'crimson'
    }

    MapRectangle {
        id: groupRectangle
        topLeft: QtPositioning.coordinate(parent.latitude + 5, parent.longitude - 5)
        bottomRight: QtPositioning.coordinate(parent.latitude, parent.longitude )
        color: 'yellow'
    }
}
