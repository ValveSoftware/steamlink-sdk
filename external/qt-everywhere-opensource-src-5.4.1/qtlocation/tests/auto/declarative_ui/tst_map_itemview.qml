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
import QtLocation 5.3
import QtPositioning 5.2
import QtLocation.test 5.0

Item {
    id: masterItem
    width: 200
    height: 350
    // General-purpose elements for the test:
    Plugin { id: testPlugin; name : "qmlgeo.test.plugin"; allowExperimental: true }

    property variant mapDefaultCenter: QtPositioning.coordinate(10, 30)

    Map {
        id: map
        objectName: 'staticallyDeclaredMap'
        center: mapDefaultCenter;
        plugin: testPlugin;
        width: 100
        height: 100
        zoomLevel: 2
        MapCircle {
            id: prepopulatedCircle
            objectName: 'prepopulatedCircle'
            center: mapDefaultCenter;
            radius: 100
        }
    }

    Map {
        id: map3
        objectName: 'staticallyDeclaredMapWithView'
        center: mapDefaultCenter;
        plugin: testPlugin;
        width: 100
        height: 100
        zoomLevel: 2
        MapItemView {
            id: theItemView3
            model: testModel3
            delegate: Component {
                MapCircle {
                    radius: 1500000
                    center {
                        latitude: modeldata.coordinate.latitude
                        longitude: modeldata.coordinate.longitude
                    }
                }
            }
        }
    }

    MapCircle {
        id: externalCircle
        objectName: 'externalCircle'
        radius: 200
        center: mapDefaultCenter
    }

    SignalSpy {id: mapItemSpy; target: map; signalName: 'mapItemsChanged'}


    MapCircle {
        objectName: "externalCircle2"
        id: externalCircle2
        radius: 2000000
        center: mapDefaultCenter
    }

    MapCircle {
        objectName: "externalCircle3"
        id: externalCircle3
        radius: 2000000
        center: mapDefaultCenter
    }

    MapRectangle {
        objectName: "externalRectangle"
        id: externalRectangle
    }

    MapPolygon {
        objectName: "externalPolygon"
        id: externalPolygon
    }

    MapPolyline {
        objectName: 'externalPolyline'
        id: externalPolyline
    }

    MapQuickItem {
        objectName: 'externalQuickItem'
        id: externalQuickItem
        sourceItem: Rectangle {}
    }

    TestModel {
        id: testModel
        datatype: 'coordinate'
        datacount: 7
        delay: 0
    }

    TestModel {
        id: testModel2
        datatype: 'coordinate'
        datacount: 3
        delay: 0
    }

    TestModel {
        id: testModel3
        datatype: 'coordinate'
        datacount: 0
        delay: 0
    }

    Plugin {
        id: testPlugin_immediate;
        name: "qmlgeo.test.plugin"
        allowExperimental: true
        parameters: [
            // Parms to guide the test plugin
            PluginParameter { name: "gc_supported"; value: true},
            PluginParameter { name: "gc_finishRequestImmediately"; value: true},
            PluginParameter { name: "gc_validateWellKnownValues"; value: true}
        ]
    }
    RouteQuery {id: routeQuery;
        waypoints: [
            { latitude: 60, longitude: 60 },
            { latitude: 61, longitude: 62 },
            { latitude: 63, longitude: 64 },
            { latitude: 65, longitude: 66 },
            { latitude: 67, longitude: 68 }
        ]
    }

    RouteModel {id: routeModel; plugin: testPlugin_immediate; query: routeQuery }
    SignalSpy {id: mapItemsChangedSpy; target: mapForTestingRouteModel; signalName: "mapItemsChanged"}

    Map {
        id: mapForView;
        center: mapDefaultCenter;
        plugin: testPlugin;
        anchors.fill: parent;
        zoomLevel: 2
        MapCircle {
            id: internalCircle
            radius: 2000000
            center: mapDefaultCenter
        }
        MapItemView {
            id: theItemView
            model: testModel
            delegate: Component {
                id: theItemViewsComponent
                MapCircle {
                    radius: 1500000
                    center {
                        latitude: modeldata.coordinate.latitude
                        longitude: modeldata.coordinate.longitude
                    }
                }
            }
        }
    }

    Map {
        id: mapForTestingListModel;
        center: mapDefaultCenter;
        plugin: testPlugin;
        anchors.fill: parent;
        zoomLevel: 2

        property variant firstItemCoord: QtPositioning.coordinate(11, 31)
        property variant secondItemCoord: QtPositioning.coordinate(12, 32)

        MapItemView {
            id: listModelItemView
            model: ListModel {
                id: testingListModel
                ListElement { lat: 11; lon: 31 }
                ListElement { lat: 12; lon: 32 }
                ListElement { lat: 13; lon: 33 }
            }
            delegate: Component {
                MapCircle {
                    radius: 1500000
                    center {
                        latitude: lat
                        longitude: lon
                    }
                }
            }
        }
    }

    Map {
        id: mapForTestingRouteModel;
        plugin: testPlugin;
        center: mapDefaultCenter;
        anchors.fill: parent;
        zoomLevel: 2
        MapItemView {
            id: routeItemView
            model: routeModel
            delegate: Component {
                MapRoute {
                    route:  routeData
                }
            }
        }
    }

    TestCase {
        name: "MapItem"
        when: windowShown
        function clear_data() {
            mapItemSpy.clear()
        }

        function test_basics() {
            compare(theItemView.delegate, theItemViewsComponent);
            compare(theItemView.model, testModel);
        }

        function test_aaa_basic_add_remove() { // aaa to ensure execution first
            clear_data()
            compare(map.mapItems.length, 1)
            compare(map.mapItems[0], prepopulatedCircle)
            compare(mapItemSpy.count, 0)
            // nonexistent
            map.removeMapItem(externalCircle)
            compare(mapItemSpy.count, 0)
            compare(map.mapItems.length, 1)
            compare(map.mapItems[0], prepopulatedCircle)
            // real
            map.removeMapItem(prepopulatedCircle)
            compare(mapItemSpy.count, 1)
            compare(map.mapItems.length, 0)
            map.addMapItem(externalCircle)
            map.addMapItem(prepopulatedCircle)
            compare(mapItemSpy.count, 3)
            compare(map.mapItems.length, 2)
            // same again
            map.addMapItem(prepopulatedCircle)
            compare(mapItemSpy.count, 3)
            compare(map.mapItems.length, 2)
            compare(map.mapItems[0], externalCircle)
            compare(map.mapItems[1], prepopulatedCircle)
            map.removeMapItem(externalCircle)
            compare(map.mapItems[0], prepopulatedCircle)
            compare(mapItemSpy.count, 4)
            compare(map.mapItems.length, 1)
            map.clearMapItems()
            compare(mapItemSpy.count, 5)
            compare(map.mapItems.length, 0)
            // empty map, do not crash
            map.clearMapItems()
            compare(mapItemSpy.count, 5)
            compare(map.mapItems.length, 0)
        }

        function test_dynamic_map_and_items() {
            clear_data();
            /*
            // basic create-destroy without items, mustn't crash
            var dynamicMap = Qt.createQmlObject('import QtQuick 2.0; import QtLocation 5.3; Map { x:0; y:0; objectName: \'dynomik map\'; width: masterItem.width; height: masterItem.height; plugin: testPlugin} ', masterItem, "dynamicCreationErrors" );
            verify(dynamicMap !== null)
            dynamicMap.destroy(1)
            //wait(5)

            // add rm add, destroy with item on it
            dynamicMap = Qt.createQmlObject('import QtQuick 2.0; import QtLocation 5.3; Map { x:0; y:0; objectName: \'dynomik map\'; width: masterItem.width; height: masterItem.height; plugin: testPlugin} ', masterItem, "dynamicCreationErrors" );
            verify(dynamicMap !== null)
            dynamicMap.addMapItem(externalCircle);
            compare(dynamicMap.mapItems.length, 1)
            dynamicMap.removeMapItem(externalCircle);
            compare(dynamicMap.mapItems.length, 0)
            dynamicMap.addMapItem(externalCircle);
            compare(dynamicMap.mapItems.length, 1)
            dynamicMap.destroy(1)
            //wait(5)

            // try adding same item to two maps, will not be allowed
            var dynamicMap2 = Qt.createQmlObject('import QtQuick 2.0; import QtLocation 5.3; Map { x:0; y:0; objectName: \'dynomik map2\'; width: masterItem.width; height: masterItem.height; plugin: testPlugin} ', masterItem, "dynamicCreationErrors" );
            dynamicMap = Qt.createQmlObject('import QtQuick 2.0; import QtLocation 5.3; Map { x:0; y:0; objectName: \'dynomik map\'; width: masterItem.width; height: masterItem.height; plugin: testPlugin} ', masterItem, "dynamicCreationErrors" );
            verify(dynamicMap !== null)
            verify(dynamicMap2 !== null)
            compare(dynamicMap.mapItems.length, 0)
            dynamicMap.addMapItem(externalCircle3);
            compare(dynamicMap.mapItems.length, 1)
            dynamicMap2.addMapItem(externalCircle3);
            compare(dynamicMap2.mapItems.length, 0)

            // create and destroy a dynamic item that is in the map
            var dynamicCircle = Qt.createQmlObject('import QtQuick 2.0; import QtLocation 5.3; MapCircle { objectName: \'dynamic circle 1\'; center { latitude: 5; longitude: 5 } radius: 15 } ', masterItem, "dynamicCreationErrors" );
            verify (dynamicCircle !== null)
            compare(map.mapItems.length, 0)
            map.addMapItem(dynamicCircle)
            compare(mapItemSpy.count, 1)
            compare(map.mapItems.length, 1)
            dynamicCircle.destroy(1)
            tryCompare(mapItemSpy, "count", 2)
            compare(map.mapItems.length, 0)

            // leave one map item, will be destroyed at the end of the case
            dynamicMap.addMapItem(externalCircle);
            compare(dynamicMap.mapItems.length, 2)

            // leave a handful of item from model to the map and let it destroy
            compare(map3.mapItems.length, 0)
            testModel3.datacount = 4
            testModel3.update()
            compare(map3.mapItems.length, 4)
            */
        }

        function test_add_and_remove_with_view() {
            // Basic adding and removing of static object
            compare(mapForView.mapItems.length, 8) // 1 declared and 7 from model
            mapForView.addMapItem(internalCircle)
            compare(mapForView.mapItems.length, 8)
            mapForView.removeMapItem(internalCircle)
            compare(mapForView.mapItems.length, 7)
            mapForView.removeMapItem(internalCircle)
            compare(mapForView.mapItems.length, 7)
            // Basic adding and removing of dynamic object
            var dynamicCircle = Qt.createQmlObject( "import QtQuick 2.0; import QtLocation 5.3; MapCircle {radius: 4000; center: mapDefaultCenter}", map, "");
            mapForView.addMapItem(dynamicCircle)
            compare(mapForView.mapItems.length, 8)
            mapForView.removeMapItem(dynamicCircle)
            compare(mapForView.mapItems.length, 7)
            mapForView.removeMapItem(dynamicCircle)
            compare(mapForView.mapItems.length, 7)
        }
        SignalSpy {id: model1Spy; target: testModel; signalName: "modelChanged"}
        SignalSpy {id: model2Spy; target: testModel2; signalName: "modelChanged"}
        function test_model_change() {
            // Change the model of an MapItemView on the fly
            // and verify that object counts change accordingly.
            testModel.datacount = 7
            testModel.update()
            compare(mapForView.mapItems.length, 7)
            testModel.datacount += 2
            testModel2.datacount += 1
            compare(mapForView.mapItems.length, 9)
            theItemView.model = testModel
            compare(mapForView.mapItems.length, 9)
            theItemView.model = testModel2
            compare(mapForView.mapItems.length, 4)
        }

        function test_listmodel() {
            compare(mapForTestingListModel.mapItems.length, 3);
            compare(mapForTestingListModel.mapItems[0].center.longitude,
                    mapForTestingListModel.firstItemCoord.longitude);
            compare(mapForTestingListModel.mapItems[0].center.latitude,
                    mapForTestingListModel.firstItemCoord.latitude);
            testingListModel.remove(0);
            compare(mapForTestingListModel.mapItems.length, 2);
            compare(mapForTestingListModel.mapItems[0].center.longitude,
                    mapForTestingListModel.secondItemCoord.longitude);
            compare(mapForTestingListModel.mapItems[0].center.latitude,
                    mapForTestingListModel.secondItemCoord.latitude);
            testingListModel.append({ lat: 1, lon: 1 });
            compare(mapForTestingListModel.mapItems.length, 3);
            compare(mapForTestingListModel.mapItems[2].center.latitude, 1);
            testingListModel.clear();
            compare(mapForTestingListModel.mapItems.length, 0);
        }

        function test_routemodel() {
            testModel.reset();
            mapItemsChangedSpy.clear()
            compare(mapForTestingRouteModel.mapItems.length, 0) // precondition
            compare(mapItemsChangedSpy.count, 0)
            routeQuery.numberAlternativeRoutes = 4
            routeModel.update();
            compare(mapForTestingRouteModel.mapItems.length, 4)
            routeQuery.numberAlternativeRoutes = 3
            routeModel.update();
            compare(mapForTestingRouteModel.mapItems.length, 3)
            routeModel.reset();
            compare(mapForTestingRouteModel.mapItems.length, 0)
            routeModel.reset(); // clear empty model
            routeQuery.numberAlternativeRoutes = 3
            routeModel.update();
            compare(mapForTestingRouteModel.mapItems.length, 3)
            mapForTestingRouteModel.addMapItem(externalCircle2)
            compare(mapForTestingRouteModel.mapItems.length, 4)
            compare(mapForTestingRouteModel.mapItems[3], externalCircle2)
            routeModel.reset();
            compare(mapForTestingRouteModel.mapItems.length, 1)
            mapForTestingRouteModel.clearMapItems()
            compare(mapForTestingRouteModel.mapItems.length, 0)

            // Test the mapItems list
            mapForTestingRouteModel.addMapItem(externalCircle2)
            compare(mapForTestingRouteModel.mapItems.length, 1)
            compare(mapForTestingRouteModel.mapItems[0], externalCircle2)

            mapForTestingRouteModel.addMapItem(externalRectangle)
            compare(mapForTestingRouteModel.mapItems.length, 2)
            compare(mapForTestingRouteModel.mapItems[1], externalRectangle)

            mapForTestingRouteModel.addMapItem(externalRectangle)
            compare(mapForTestingRouteModel.mapItems.length, 2)
            compare(mapForTestingRouteModel.mapItems[1], externalRectangle)

            mapForTestingRouteModel.addMapItem(externalPolygon)
            compare(mapForTestingRouteModel.mapItems.length, 3)
            compare(mapForTestingRouteModel.mapItems[2], externalPolygon)

            mapForTestingRouteModel.addMapItem(externalQuickItem)
            compare(mapForTestingRouteModel.mapItems.length, 4)
            compare(mapForTestingRouteModel.mapItems[3], externalQuickItem)

            mapForTestingRouteModel.removeMapItem(externalCircle2)
            compare(mapForTestingRouteModel.mapItems.length, 3)
            compare(mapForTestingRouteModel.mapItems[0], externalRectangle)

            mapForTestingRouteModel.removeMapItem(externalRectangle)
            compare(mapForTestingRouteModel.mapItems.length, 2)
            compare(mapForTestingRouteModel.mapItems[0], externalPolygon)

            mapForTestingRouteModel.clearMapItems()
            compare(mapForTestingRouteModel.mapItems.length, 0)
        }
    }
}
