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
import QtLocation 5.6
import QtPositioning 5.5

Item {
    id: page
    x: 0; y: 0;
    width: 200
    height: 100
    property variant coordinate: QtPositioning.coordinate(20, 20)

    Plugin {
        id: errorPlugin; name: "qmlgeo.test.plugin"; allowExperimental: true
        parameters: [
            PluginParameter { name: "error"; value: "1"},
            PluginParameter { name: "errorString"; value: "This error was expected. No worries !"}
        ]
    }

    Map {
        id: map_error_plugin;
        property alias mouseClickedSpy: mouseClickedSpy1
        x: 0; y: 0; width: 100; height: 100; plugin: errorPlugin;

        MouseArea {
            id: mouseArea1
            objectName: "mouseArea"
            x: 25; y: 25; width: 50; height: 50;
            preventStealing: true
        }

        SignalSpy {id: mouseClickedSpy1; target: mouseArea1; signalName: "clicked"}
    }

    Map {
        id: map_no_plugin;
        property alias mouseClickedSpy: mouseClickedSpy2
        x: 100; y: 0; width: 100; height: 100;

        MouseArea {
            id: mouseArea2
            objectName: "mouseArea"
            x: 25; y: 25; width: 50; height: 50;
            preventStealing: true
        }

        SignalSpy {id: mouseClickedSpy2; target: mouseArea2; signalName: "clicked"}
    }

    TestCase {
        name: "MapErrorHandling"
        when: windowShown

        function init() {
            map_error_plugin.zoomLevel = 0
            map_no_plugin.zoomLevel = 0
            map_error_plugin.center = QtPositioning.coordinate(0, 0)
            map_no_plugin.center = QtPositioning.coordinate(0, 0)
            map_error_plugin.mouseClickedSpy.clear()
            map_no_plugin.mouseClickedSpy.clear()
        }

        function map_clicked(map)
        {
            mouseClick(map, 5, 5)
            mouseClick(map, 50, 50)
            mouseClick(map, 50, 50)
            mouseClick(map, 50, 50)
            tryCompare(map.mouseClickedSpy, "count", 3)
        }

        function test_map_clicked_wiht_no_plugin()
        {
            map_clicked(map_no_plugin)
        }

        function test_map_clicked_with_error_plugin()
        {
            map_clicked(map_error_plugin)
        }

        function test_map_no_supportedMapTypes()
        {
            compare(map_no_plugin.supportedMapTypes.length , 0)
            compare(map_error_plugin.supportedMapTypes.length , 0)
        }

        function test_map_set_zoom_level()
        {
            map_no_plugin.zoomLevel = 9
            compare(map_no_plugin.zoomLevel,9)
            map_error_plugin.zoomLevel = 9
            compare(map_error_plugin.zoomLevel,9)
        }

        function test_map_set_center()
        {
            map_no_plugin.center = coordinate
            verify(map_no_plugin.center === coordinate)
            map_error_plugin.center = coordinate
            verify(map_error_plugin.center === coordinate)
        }

        function test_map_no_mapItems()
        {
            compare(map_no_plugin.mapItems.length , 0)
            compare(map_error_plugin.mapItems.length , 0)
        }

        function test_map_error()
        {
            compare(map_no_plugin.error , 0)
            compare(map_no_plugin.errorString , "")
            compare(map_error_plugin.error , 1)
            compare(map_error_plugin.errorString ,"This error was expected. No worries !")
        }

        function test_map_toCoordinate()
        {
            map_no_plugin.center = coordinate
            compare(map_no_plugin.toCoordinate(50,50).isValid,false)
            map_error_plugin.center = coordinate
            compare(map_error_plugin.toCoordinate(50,50).isValid,false)
        }

        function test_map_fromCoordinate()
        {
            verify(isNaN(map_error_plugin.fromCoordinate(coordinate).x))
            verify(isNaN(map_error_plugin.fromCoordinate(coordinate).y))
            verify(isNaN(map_no_plugin.fromCoordinate(coordinate).x))
            verify(isNaN(map_no_plugin.fromCoordinate(coordinate).y))
        }

        function test_map_gesture_enabled()
        {
            verify(map_error_plugin.gesture.enabled)
            verify(map_no_plugin.gesture.enabled)
        }

        function test_map_pan()
        {
            map_no_plugin.center = coordinate
            map_no_plugin.pan(20,20)
            verify(map_no_plugin.center === coordinate)
            map_error_plugin.center = coordinate
            map_error_plugin.pan(20,20)
            verify(map_error_plugin.center === coordinate)
        }

        function test_map_prefetchData()
        {
            map_error_plugin.prefetchData()
            map_no_plugin.prefetchData()
        }

        function test_map_fitViewportToMapItems()
        {
            map_error_plugin.fitViewportToMapItems()
            map_no_plugin.fitViewportToMapItems()
        }

        function test_map_setVisibleRegion()
        {
            map_no_plugin.visibleRegion = QtPositioning.circle(coordinate,1000)
            verify(map_no_plugin.center != coordinate)
            verify(map_no_plugin.visibleRegion == QtPositioning.circle(coordinate,1000))
            map_error_plugin.visibleRegion = QtPositioning.circle(coordinate,1000)
            verify(map_error_plugin.center != coordinate)
            verify(map_no_plugin.visibleRegion == QtPositioning.circle(coordinate,1000))
        }

        function test_map_activeMapType()
        {
            compare(map_no_plugin.supportedMapTypes.length, 0)
            compare(map_no_plugin.activeMapType.style, MapType.NoMap)
            compare(map_error_plugin.supportedMapTypes.length, 0)
            compare(map_error_plugin.activeMapType.style, MapType.NoMap)
        }
    }
}
