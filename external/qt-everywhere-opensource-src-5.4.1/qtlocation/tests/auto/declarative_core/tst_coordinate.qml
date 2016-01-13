/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

import QtQuick 2.0
import QtTest 1.0
import QtPositioning 5.2

Item {
    id: item

    property variant empty: QtPositioning.coordinate()
    property variant base: QtPositioning.coordinate(1.0, 1.0, 5.0)
    property variant zero: QtPositioning.coordinate(0, 0)
    property variant plusone: QtPositioning.coordinate(0, 1)
    property variant minusone: QtPositioning.coordinate(0, -1)
    property variant north: QtPositioning.coordinate(3, 0)

    SignalSpy { id: coordSpy; target: item; signalName: "baseChanged" }

    property variant inside: QtPositioning.coordinate(0.5, 0.5)
    property variant outside: QtPositioning.coordinate(2, 2)
    property variant tl: QtPositioning.coordinate(1, 0)
    property variant br: QtPositioning.coordinate(0, 1)
    property variant box: QtPositioning.rectangle(tl, br)


    Address {
        id: validTestAddress
        street: "53 Brandl St"
        city: "Eight Mile Plains"
        country: "Australia"
        countryCode: "AUS"
    }

    Location {
        id: testLocation
        coordinate: inside
        boundingBox: box
        address: validTestAddress
    }

    Location {
        id: invalidLocation
    }

    TestCase {
        name: "GeoLocation"

        function test_Location_complete() {
            compare (testLocation.coordinate.longitude, inside.longitude)
            compare (testLocation.coordinate.latitude, inside.latitude)

            compare (testLocation.boundingBox.contains(inside), true)
            compare (testLocation.boundingBox.contains(outside), false)
            compare (testLocation.boundingBox.bottomRight.longitude, br.longitude)
            compare (testLocation.boundingBox.bottomRight.latitude, br.latitude)
            compare (testLocation.boundingBox.topLeft.longitude, tl.longitude)
            compare (testLocation.boundingBox.topLeft.latitude, tl.latitude)

            compare (testLocation.address.country, "Australia")
            compare (testLocation.address.countryCode, "AUS")
            compare (testLocation.address.city, "Eight Mile Plains")
            compare (testLocation.address.street, "53 Brandl St")
        }

        function test_Location_invalid() {
            compare(invalidLocation.coordinate.isValid, false)
            compare(invalidLocation.boundingBox.isEmpty, true)
            compare(invalidLocation.boundingBox.isValid, false)
            compare(invalidLocation.address.city, "")
        }
    }

    TestCase {
        name: "Coordinate"

        function test_validity() {
            compare(empty.isValid, false)

            empty.longitude = 0.0;
            empty.latitude = 0.0;

            compare(empty.isValid, true)
        }

        function test_accessors() {
            compare(base.longitude, 1.0)
            compare(base.latitude, 1.0)
            compare(base.altitude, 5.0)

            coordSpy.clear();

            base.longitude = 2.0;
            base.latitude = 3.0;
            base.altitude = 6.0;

            compare(base.longitude, 2.0)
            compare(base.latitude, 3.0)
            compare(base.altitude, 6.0)
            compare(coordSpy.count, 3)
        }

        function test_comparison_data() {
            return [
                { tag: "empty", coord1: empty, coord2: QtPositioning.coordinate(), result: true },
                { tag: "zero", coord1: zero, coord2: QtPositioning.coordinate(0, 0), result: true },
                { tag: "plusone", coord1: plusone, coord2: QtPositioning.coordinate(0, 1), result: true },
                { tag: "minusone", coord1: minusone, coord2: QtPositioning.coordinate(0, -1), result: true },
                { tag: "north", coord1: north, coord2: QtPositioning.coordinate(3, 0), result: true },
                { tag: "lat,long.alt", coord1: QtPositioning.coordinate(1.1, 2.2, 3.3), coord2: QtPositioning.coordinate(1.1, 2.2, 3.3), result: true },
                { tag: "not equal1", coord1: plusone, coord2: minusone, result: false },
                { tag: "not equal2", coord1: plusone, coord2: north, result: false }
            ]
        }

        function test_comparison(data) {
            compare(data.coord1 === data.coord2, data.result)
            compare(data.coord1 !== data.coord2, !data.result)
            compare(data.coord1 == data.coord2, data.result)
            compare(data.coord1 != data.coord2, !data.result)
        }

        function test_distance() {
            compare(zero.distanceTo(plusone), zero.distanceTo(minusone))
            compare(2*plusone.distanceTo(zero), plusone.distanceTo(minusone))
            compare(zero.distanceTo(plusone) > 0, true)
        }

        function test_azimuth() {
            compare(zero.azimuthTo(north), 0)
            compare(zero.azimuthTo(plusone), 90)
            compare(zero.azimuthTo(minusone), 270)
            compare(minusone.azimuthTo(plusone), 360 - plusone.azimuthTo(minusone))
        }

        function test_atDistanceAndAzimuth() {
            // 112km is approximately one degree of arc

            var coord_0d = zero.atDistanceAndAzimuth(112000, 0)
            compare(coord_0d.latitude > 0.95, true)
            compare(coord_0d.latitude < 1.05, true)
            compare(coord_0d.longitude < 0.05, true)
            compare(coord_0d.longitude > -0.05, true)
            compare(zero.distanceTo(coord_0d), 112000)
            compare(zero.azimuthTo(coord_0d), 0)

            var coord_90d = zero.atDistanceAndAzimuth(112000, 90)
            compare(coord_90d.longitude > 0.95, true)
            compare(coord_90d.longitude < 1.05, true)
            compare(coord_90d.latitude < 0.05, true)
            compare(coord_90d.latitude > -0.05, true)
            compare(zero.distanceTo(coord_90d), 112000)
            compare(zero.azimuthTo(coord_90d), 90)

            var coord_30d = zero.atDistanceAndAzimuth(20000, 30)
            compare(coord_30d.longitude > 0, true)
            compare(coord_30d.latitude > 0, true)
            compare(zero.distanceTo(coord_30d), 20000)
            compare(zero.azimuthTo(coord_30d), 30)

            var coord_30d2 = coord_30d.atDistanceAndAzimuth(200, 30)
            compare(zero.distanceTo(coord_30d2), 20200)
        }
    }
}
