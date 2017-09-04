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
import QtTest 1.0
import QtPositioning 5.5

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


    Item {
        id: coordinateItem
        property variant coordinate
        property int animationDuration: 100
        property var coordinateList: []
        property int coordinateCount: 0

        CoordinateAnimation {
            id: coordinateAnimation
            target: coordinateItem
            property: "coordinate"
            duration: coordinateItem.animationDuration
        }
        onCoordinateChanged: {
            if (!coordinateList) {
                coordinateList = []
            }
            coordinateList[coordinateCount] = QtPositioning.coordinate(coordinate.latitude,coordinate.longitude)
            coordinateCount++
        }

        SignalSpy { id: coordinateAnimationStartSpy; target: coordinateAnimation; signalName: "started" }
        SignalSpy { id: coordinateAnimationStopSpy; target: coordinateAnimation; signalName: "stopped" }
        SignalSpy { id: coordinateAnimationDirectionSpy; target: coordinateAnimation; signalName: "directionChanged" }
    }

    TestCase {
        name: "GeoLocation"

        function test_Location_complete()
        {
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

        function test_Location_invalid()
        {
            compare(invalidLocation.coordinate.isValid, false)
            compare(invalidLocation.boundingBox.isEmpty, true)
            compare(invalidLocation.boundingBox.isValid, false)
            compare(invalidLocation.address.city, "")
        }
    }

    TestCase {
        name: "Coordinate"

        function test_validity()
        {
            compare(empty.isValid, false)

            empty.longitude = 0.0;
            empty.latitude = 0.0;

            compare(empty.isValid, true)
        }

        function test_accessors()
        {
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

        function test_comparison_data()
        {
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

        function test_comparison(data)
        {
            compare(data.coord1 === data.coord2, data.result)
            compare(data.coord1 !== data.coord2, !data.result)
            compare(data.coord1 == data.coord2, data.result)
            compare(data.coord1 != data.coord2, !data.result)
        }

        function test_distance()
        {
            compare(zero.distanceTo(plusone), zero.distanceTo(minusone))
            compare(2*plusone.distanceTo(zero), plusone.distanceTo(minusone))
            compare(zero.distanceTo(plusone) > 0, true)
        }

        function test_azimuth()
        {
            compare(zero.azimuthTo(north), 0)
            compare(zero.azimuthTo(plusone), 90)
            compare(zero.azimuthTo(minusone), 270)
            compare(minusone.azimuthTo(plusone), 360 - plusone.azimuthTo(minusone))
        }

        function test_atDistanceAndAzimuth()
        {
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

    TestCase {
        name: "CoordinateAnimation"

        function init()
        {
            coordinateAnimation.stop()
            coordinateAnimationStartSpy.clear()
            coordinateAnimationStopSpy.clear()
            coordinateAnimationDirectionSpy.clear()
            coordinateAnimation.from = QtPositioning.coordinate(50,50)
            coordinateAnimation.to = QtPositioning.coordinate(50,50)
            coordinateAnimation.direction = CoordinateAnimation.Shortest
            coordinateItem.coordinate = QtPositioning.coordinate(50,50)
            coordinateItem.coordinateList = []
            coordinateItem.coordinateCount = 0
        }

        function initTestCase()
        {
            compare(coordinateAnimation.direction, CoordinateAnimation.Shortest)
            compare(coordinateAnimationDirectionSpy.count,0)
            coordinateAnimation.direction = CoordinateAnimation.Shortest
            compare(coordinateAnimationDirectionSpy.count,0)
            coordinateAnimation.direction = CoordinateAnimation.West
            compare(coordinateAnimationDirectionSpy.count,1)
            coordinateAnimation.direction = CoordinateAnimation.East
            compare(coordinateAnimationDirectionSpy.count,2)
        }

        function toMercator(coord)
        {
            var pi = Math.PI
            var lon = coord.longitude / 360.0 + 0.5;

            var lat = coord.latitude;
            lat = 0.5 - (Math.log(Math.tan((pi / 4.0) + (pi / 2.0) * lat / 180.0)) / pi) / 2.0;
            lat = Math.max(0.0, lat);
            lat = Math.min(1.0, lat);

            return {'latitude': lat, 'longitude': lon};
        }

        function coordinate_animation(from, to, movingEast)
        {
            var fromMerc = toMercator(from)
            var toMerc = toMercator(to)
            var delta = (toMerc.latitude - fromMerc.latitude) / (toMerc.longitude - fromMerc.longitude)

            compare(coordinateItem.coordinateList.length, 0);
            coordinateAnimation.from =  from
            coordinateAnimation.to = to
            coordinateAnimation.start()
            tryCompare(coordinateAnimationStartSpy,"count",1)
            tryCompare(coordinateAnimationStopSpy,"count",1)

            //check correct start position
            compare(coordinateItem.coordinateList[0], from)
            //check correct end position
            compare(coordinateItem.coordinateList[coordinateItem.coordinateList.length - 1],to)

            var i
            var lastLongitude
            for (i in coordinateItem.coordinateList) {
                var coordinate = coordinateItem.coordinateList[i]
                var mercCoordinate = toMercator(coordinate)

                //check that coordinates from the animation is along a straight line between from and to
                var estimatedLatitude = fromMerc.latitude + (mercCoordinate.longitude - fromMerc.longitude) * delta
                verify(mercCoordinate.latitude - estimatedLatitude < 0.00000000001);

                //check that each step has moved in the right direction

                if (lastLongitude) {
                    if (movingEast) {
                        if (coordinate.longitude > 0 && lastLongitude < 0)
                            verify(coordinate.longitude < lastLongitude + 360)
                        else
                            verify(coordinate.longitude < lastLongitude)
                    } else {
                        if (coordinate.longitude < 0 && lastLongitude > 0)
                            verify(coordinate.longitude + 360 > lastLongitude)
                        else
                            verify(coordinate.longitude > lastLongitude)
                    }
                }
                lastLongitude = coordinate.longitude
            }
        }

        function test_default_coordinate_animation()
        {
            //shortest
            coordinate_animation(QtPositioning.coordinate(58.0,12.0),
                                 QtPositioning.coordinate(62.0,24.0),
                                 false)
        }

        function test_east_direction_coordinate_animation(data)
        {
            coordinateAnimation.direction = CoordinateAnimation.East
            coordinate_animation(data.from,
                                 data.to,
                                 true)
        }

        function test_east_direction_coordinate_animation_data()
        {
            return [
                { from: QtPositioning.coordinate(58.0,24.0), to: QtPositioning.coordinate(58.0,12.0) },
                { from: QtPositioning.coordinate(58.0,12.0), to: QtPositioning.coordinate(58.0,24.0) },
            ]
        }


        function test_west_direction_coordinate_animation(data)
        {
            coordinateAnimation.direction = CoordinateAnimation.West
            coordinate_animation(data.from,
                                 data.to,
                                 false)
        }

        function test_west_direction_coordinate_animation_data()
        {
            return [
                { from: QtPositioning.coordinate(58.0,24.0),to: QtPositioning.coordinate(58.0,12.0) },
                { from: QtPositioning.coordinate(58.0,12.0),to: QtPositioning.coordinate(58.0,24.0) },
            ]
        }


    }
}
