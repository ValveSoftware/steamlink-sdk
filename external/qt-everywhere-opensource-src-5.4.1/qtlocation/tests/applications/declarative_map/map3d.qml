/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtPositioning 5.2
import QtLocation 5.3
import QtLocation.test 5.0
import "common" as Common

//import Qt3D 1.0
//import Qt.multimediakit 4.0

Rectangle {
    objectName: "The page."
    color: 'yellow'
    width:  1140
    height: 1085
    id: page

    // From location.test plugin
    TestModel {
          id: testModel
          datatype: 'coordinate'
          datacount: 8
          delay: 0
          crazyMode:  false  // generate arbitrarily updates. interval is set below, and the number of items is varied between 0..datacount
          crazyLevel: 2000 // the update interval varies between 3...crazyLevel (ms)
      }

    Column {
        id: buttonColumn
        anchors.top: page.top
        anchors.left:  map.right
        spacing: 2

        Rectangle {color: "lightblue"; width: 80; height: 80;
            Text {text: "Crazy mode:\n" + testModel.crazyMode + "\nclick to\ntoggle."}
            MouseArea{ anchors.fill: parent;
                onClicked: testModel.crazyMode = !testModel.crazyMode
                onDoubleClicked: map.removeMapItem(mapItem1)
            }
        }
        AnimatedImage {
            MouseArea { anchors.fill: parent; onClicked: mapItem2.source = parent }
            width: 80
            height: 80
            playing: testModel.crazyMode
            source: "blinky.gif"
        }
        Rectangle {color: "lightblue"; width: 80; height: 80;
            Text {text: "Click:\nadd item1\nDouble-click:\nrm item1"}
            MouseArea{ anchors.fill: parent;
                onClicked: {console.log('----------------adding item 1'); map.addMapItem(externalStaticMapItem1);}
                onDoubleClicked: {console.log('+++++++++++++++ removing item 1'); map.removeMapItem(externalStaticMapItem1);}
            }
        }
        Rectangle {color: "lightblue"; width: 80; height: 40;
            Text {text: "Add\nitem"}
            MouseArea{ anchors.fill: parent; onClicked: map.addMapItem(extMapCircle)}
        }
        Rectangle {color: "lightblue"; width: 80; height: 80;
            Text {text: "Click:\nadd item2\nDouble-click:\nrm item2"}
            MouseArea{ anchors.fill: parent;
                onClicked: {console.log('adding item 2'); map.addMapItem(externalStaticMapItem2);}
                onDoubleClicked: {console.log('removing item 2'); map.removeMapItem(externalStaticMapItem2);}
            }
        }
        Rectangle {color: "lightblue"; width: 80; height: 40;
            Text {text: "Click:\nlat++"}
            MouseArea{ anchors.fill: parent;
                onClicked: { mapCenterCoordinate.latitude += 1}
            }
        }
        Rectangle {color: "lightblue"; width: 80; height: 40;
            Text {text: "Click:\nlat--"}
            MouseArea{ anchors.fill: parent;
                onClicked: { mapCenterCoordinate.latitude -= 1}
            }
        }
        Rectangle {color: "lightblue"; width: 80; height: 40;
            Text {text: "Click:\nlon++"}
            MouseArea{ anchors.fill: parent;
                onClicked: { mapCenterCoordinate.longitude += 1}
            }
        }
        Rectangle {color: "lightblue"; width: 80; height: 40;
            Text {text: "Click:\nlon--"}
            MouseArea{ anchors.fill: parent;
                onClicked: { mapCenterCoordinate.longitude -= 1}
            }
        }
        Rectangle {color: "lightblue"; width: 80; height: 40;
            Text {text: "Click:\nalt++"}
            MouseArea{ anchors.fill: parent;
                onClicked: { mapCenterCoordinate.altitude += 1}
            }
        }
        Rectangle {color: "lightblue"; width: 80; height: 40;
            Text {text: "Click:\nalt--"}
            MouseArea{ anchors.fill: parent;
                onClicked: { mapCenterCoordinate.altitude -= 1}
            }
        }
        Rectangle {color: "lightblue"; width: 80; height: 40;
            Text {text: "Click:\nbear++"}
            MouseArea{ anchors.fill: parent;
                onClicked: { map.bearing += 1.1}
                onDoubleClicked: { map.bearing += 20.1}
            }
        }
        Rectangle {color: "lightblue"; width: 80; height: 40;
            Text {text: "Click:\nbear--"}
            MouseArea{ anchors.fill: parent;
                onClicked: { map.bearing -= 1.1}
                onDoubleClicked: { map.bearing -= 20.1}
            }
        }
        Rectangle {color: "lightblue"; width: 80; height: 40;
            Text {text: "Click:\pinch"}
            MouseArea{ anchors.fill: parent;
                onClicked: {
                    pinchGenerator.pinch(
                                            Qt.point(100,100),   // point1From
                                            Qt.point(150,150),   // point1To
                                            Qt.point(300,300),   // point2From
                                            Qt.point(150,150),   // point2To
                                            20,                // interval between touch events (swipe1), default 20ms
                                            20,                // interval between touch events (swipe2), defualt 20ms
                                            10,                // number of touchevents in point1from -> point1to, default 10
                                            10);               // number of touchevents in point2from -> point2to, default 10
                }
            }
        }
        Rectangle {color: "lightblue"; width: 80; height: 40;
            Text {text: "Width++"}
            MouseArea{ anchors.fill: parent; onClicked: map.width += 10}
        }
        Rectangle {color: "lightblue"; width: 80; height: 40;
            Text {text: "Width--"}
            MouseArea{ anchors.fill: parent; onClicked: map.width -= 10}
        }
        Rectangle {color: "lightblue"; width: 80; height: 40;
            Text {text: "Height++"}
            MouseArea{ anchors.fill: parent; onClicked: map.height += 10}
        }
        Rectangle {color: "lightblue"; width: 80; height: 40;
            Text {text: "Height--"}
            MouseArea{ anchors.fill: parent; onClicked: map.height -= 10}
        }
        Rectangle {color: "lightblue"; width: 80; height: 40;
            Text {text: "toScrPos"}
            MouseArea{ anchors.fill: parent;
                onClicked: console.log('coordinate: ' +
                                       beyondCoordinate.latitude +
                                       ' to screen pos: ' +
                                       map.toScreenPosition(beyondCoordinate).x +
                                       ' ' + map.toScreenPosition(beyondCoordinate).y) }
        }
    }

    Coordinate {
        id: beyondCoordinate
        latitude: 80
        longitude: 80
        altitude: 0
    }

    Coordinate {
        id: brisbaneCoordinate
        latitude: -27.5
        longitude: 140
    }

    Coordinate {
        id: brisbaneCoordinate2
        latitude: -30.5
        longitude: 140
    }

    Coordinate {id: londonCoordinate3; latitude: 51.6; longitude: -0.11}

    MapCircle {
        id: extMapCircle
        z: 5
        center: londonCoordinate3
        radius: 10000
        color: 'yellow'

        MapMouseArea {
            id: mouseAreaOfExtMapCircle
            anchors.fill: parent
            drag.target: parent
            onClicked: console.log('....[extCircle].... map mouse area of extCircle clicked')
        }
        Column {
            spacing: 2
            Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                text: ' MapCircle ext '}
            Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                text: ' x: ' + extMapCircle.x}
            Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                text: ' y: ' + extMapCircle.y}
            Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black'; elide: Text.ElideRight; width: extMapCircle.width + 30;
                text: ' c lat: ' + extMapCircle.center.latitude}
            Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black'; elide: Text.ElideRight; width: extMapCircle.width + 30;
                text: ' c lon: ' + extMapCircle.center.longitude}
        }
    }


    Map {
        id: map
        property bool disableFlickOnStarted: false
        x: 10
        y: 10
        MapMouseArea {
            id: mapMouseArea
            anchors.fill: parent
            onClicked: console.log('.....[Map]..... mouse area of maps clicked, coordinate lat: ' + mapMouseArea.mouseToCoordinate(mouse).latitude +
                                   'coordinate lon: ' + mapMouseArea.mouseToCoordinate(mouse).longitude);
                                   //' to screen pos: ' + map.toScreenPosition(mapMouseAreaUpperPurple.mouseToCoordinate(mouse)).x +
                                   //' ' + map.toScreenPosition(mapMouseAreaUpperPurple.mouseToCoordinate(mouse)).y)
        }

        MapCircle {
            id: mapCircle
            z: 5
            center: londonCoordinate
            radius: 10000
            color: 'red'
            border.color: 'darkcyan'
            border.width: 15

            MapMouseArea {
                id: mouseAreaOfMapCircle
                anchors.fill: parent
                drag.target: parent
                onClicked: console.log('....[Circle].... map mouse area of circle clicked')
            }
            Column {
                spacing: 2
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                    text: ' MapCircle '}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                    text: ' x: ' + mapCircle.x}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                    text: ' y: ' + mapCircle.y}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black'; elide: Text.ElideRight; width: mapCircle.width + 30;
                    text: ' c lat: ' + mapCircle.center.latitude}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black'; elide: Text.ElideRight; width: mapCircle.width + 30;
                    text: ' c lon: ' + mapCircle.center.longitude}
            }
        }

        Coordinate { id: southEastLondonCoordinate; latitude: 51.2; longitude: 0.3 }

        MapPolyline {
            id: mapPolyline
            z: 15
            line.color: 'red'
            line.width: 10
            path: [
                Coordinate { id: pathCoord1; latitude: 50.7; longitude: 0.1},
                Coordinate { id: pathCoord2; latitude: 50.8; longitude: 0.4},
                Coordinate { id: pathCoord3; latitude: 50.9; longitude: 0.2}
            ]
            MapMouseArea {
                id: mouseAreaOfMapPolyline
                anchors.fill: parent
                drag.target: parent
                onClicked: console.log('....[Polyline].... map mouse area of polyline clicked')
            }
            Column {
                spacing: 2
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                    text: ' MapPolyline '}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                    text: ' x: ' + mapPolyline.x}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                    text: ' y: ' + mapPolyline.y}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black'; elide: Text.ElideRight; width: mapPolyline.width + 30;
                    text: ' at(0) lat: ' + mapPolyline.path[0].latitude}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black'; elide: Text.ElideRight; width: mapPolyline.width + 30;
                    text: ' at(0) lon: ' + mapPolyline.path[0].longitude}
            }
        }

        MapPolygon {
            id: mapPolygon
            color: 'pink'
            border.color: 'darkmagenta'
            border.width: 10
            path: [
                Coordinate { id: pathCoord_1; latitude: 51; longitude: 0.2},
                Coordinate { id: pathCoord_2; latitude: 51.1; longitude: 0.6},
                Coordinate { id: pathCoord_3; latitude: 51.2; longitude: 0.4}
            ]
            MapMouseArea {
                id: mouseAreaOfMapPolygon
                anchors.fill: parent
                hoverEnabled: true
                drag.target: parent
                onClicked: console.log('....[Polygon].... map mouse area of polygon clicked')
                onEntered: parent.color = 'red'
                onExited: parent.color = 'pink'
            }
            Column {
                spacing: 2
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                    text: ' MapPolygon '}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                    text: ' x: ' + mapPolygon.x}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                    text: ' y: ' + mapPolygon.y}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black'; elide: Text.ElideRight; width: mapPolygon.width + 30;
                    text: ' at(0) lat: ' + mapPolygon.path[0].latitude}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black'; elide: Text.ElideRight; width: mapPolygon.width + 30;
                    text: ' at(0) lon: ' + mapPolygon.path[0].longitude}
            }
        }

        MapRoute {
            id: mapRoute
            line.color: 'green'
            line.width: 7
            // don't do this at home - route is not user instantiable,
            // use polyline instead
            route: Route {
                path: [
                    Coordinate { latitude: 51.6; longitude: 0.2},
                    Coordinate { latitude: 51.7; longitude: 1.2},
                    Coordinate { latitude: 51.7; longitude: 1.3},
                    Coordinate { latitude: 51.8; longitude: 0.6},
                    Coordinate { latitude: 51.7; longitude: 0.6}
                ]
            }
            MapMouseArea {
                id: mouseAreaOfMapRoute
                anchors.fill: parent
                drag.target: parent
                onClicked: console.log('....[Polygon].... map mouse area of polygon clicked')
            }
            Column {
                spacing: 2
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                    text: ' MapRoute '}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                    text: ' x: ' + mapRoute.x}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                    text: ' y: ' + mapRoute.y}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black'; elide: Text.ElideRight; width: mapRoute.width + 30;
                    text: ' at(0) lat: ' + mapRoute.route.path[0].latitude}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black'; elide: Text.ElideRight; width: mapRoute.width + 30;
                    text: ' at(0) lon: ' + mapRoute.route.path[0].longitude}
            }
        }

        MapRectangle {
            id: mapRectangle
            z: 5
            topLeft: londonCoordinate
            bottomRight: southEastLondonCoordinate
            color: 'green'
            MapMouseArea {
                id: mouseAreaOfMapRectangle
                anchors.fill: parent
                drag.target: parent
                onClicked: console.log('....[Rectangle].... map mouse area of rectangle clicked')
            }
            Column {
                spacing: 2
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                    text: ' MapRectangle '}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                    text: ' x: ' + mapRectangle.x}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                    text: ' y: ' + mapRectangle.y}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black'; elide: Text.ElideRight; width: mapRectangle.width - 10;
                    text: ' tl lat: ' + mapRectangle.topLeft.latitude}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black'; elide: Text.ElideRight; width: mapRectangle.width - 10;
                    text: ' tl lon: ' + mapRectangle.topLeft.longitude}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black'; elide: Text.ElideRight; width: mapRectangle.width - 10;
                    text: ' br lat: ' + mapRectangle.bottomRight.latitude}
                Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black'; elide: Text.ElideRight; width: mapRectangle.width - 10;
                    text: ' br lon: ' + mapRectangle.bottomRight.longitude}
            }
        }

        /*
        MapQuickItem {
            objectName: "blinky quick item 1"
            coordinate: Coordinate { latitude: -19; longitude : 146 }
            sourceItem: AnimatedImage {
                width: 80
                height: 80
                playing: true
                source: "blinky.gif"
            }
        }
        */

        Coordinate {id: londonCoordinate; latitude: 51.5; longitude: -0.11}

        /*
        MapQuickItem {
            objectName: "mousetestrectanglelower yellow"
            coordinate: londonCoordinate
            MapMouseArea {
                anchors.fill: parent
                onClicked: console.log('....... yellow map mouse clicked at x: ' + mouse.x + ' y: ' + mouse.y)
            }
            sourceItem: Rectangle {
                width: 160
                height: 160
                color: 'yellow'
            }
        }
        */


        MapQuickItem {
            objectName: "mousetestrectangleupper purple"
            id: purpleRectMapItem
            coordinate: londonCoordinate

            MapMouseArea {
                id: mapMouseAreaUpperPurple
                objectName: 'mapMouseAreaUpperPurple'
                anchors.fill: parent
                //z: 100
                drag.target: parent
                onClicked: {
                    console.log('....[purple]..... purple map mouse clicked() area upper coordinate: ' + mapMouseAreaUpperPurple.mouseToCoordinate(mouse).latitude + ' to screen pos: ' + map.toScreenPosition(mapMouseAreaUpperPurple.mouseToCoordinate(mouse)).x + ' ' + map.toScreenPosition(mapMouseAreaUpperPurple.mouseToCoordinate(mouse)).y)
                }
                onPressed: {
                    console.log('....[purple]..... map mouse pressed() area purple coordinate: ' + mapMouseAreaUpperPurple.mouseToCoordinate(mouse).latitude + ' to screen pos: ' + map.toScreenPosition(mapMouseAreaUpperPurple.mouseToCoordinate(mouse)).x + ' ' + map.toScreenPosition(mapMouseAreaUpperPurple.mouseToCoordinate(mouse)).y)
                }
                onReleased: {
                    console.log('....[purple]..... map mouse released() area purple coordinate: ' + mapMouseAreaUpperPurple.mouseToCoordinate(mouse).latitude + ' to screen pos: ' + map.toScreenPosition(mapMouseAreaUpperPurple.mouseToCoordinate(mouse)).x + ' ' + map.toScreenPosition(mapMouseAreaUpperPurple.mouseToCoordinate(mouse)).y)
                }
                onDoubleClicked: {
                    console.log('....[purple]..... map mouse doubleClicked() area purple coordinate: ' + mapMouseAreaUpperPurple.mouseToCoordinate(mouse).latitude + ' to screen pos: ' + map.toScreenPosition(mapMouseAreaUpperPurple.mouseToCoordinate(mouse)).x + ' ' + map.toScreenPosition(mapMouseAreaUpperPurple.mouseToCoordinate(mouse)).y)
                }
                //onPositionChanged: {
                //    console.log('....[purple]..... map mouse positionChanged() area purple coordinate: ' + mapMouseAreaUpperPurple.mouseToCoordinate(mouse).latitude + ' to screen pos: ' + map.toScreenPosition(mapMouseAreaUpperPurple.mouseToCoordinate(mouse)).x + ' ' + map.toScreenPosition(mapMouseAreaUpperPurple.mouseToCoordinate(mouse)).y)
                //}
                onPressAndHold: {
                    console.log('....[purple]..... map mouse pressAndHold() area purple coordinate: ' + mapMouseAreaUpperPurple.mouseToCoordinate(mouse).latitude + ' to screen pos: ' + map.toScreenPosition(mapMouseAreaUpperPurple.mouseToCoordinate(mouse)).x + ' ' + map.toScreenPosition(mapMouseAreaUpperPurple.mouseToCoordinate(mouse)).y)
                }
                onEntered: {
                    console.log('....[purple]..... map mouse area purple entered()')
                }
                onExited: {
                    console.log('....[purple]..... map mouse area purple exited()')
                }
            }
            sourceItem: Rectangle {
                id: rectangleSourceItem
                width: 200
                height: 200
                color: 'purple'
                Column {
                    spacing: 2
                    Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                        text: ' MapQuickItem '}
                    Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                        text: ' w: ' + purpleRectMapItem.width}
                    Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                        text: ' h: ' + purpleRectMapItem.height}
                    Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                        text: ' x: ' + purpleRectMapItem.x}
                    Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black';
                        text: ' y: ' + purpleRectMapItem.y}
                    Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black'; elide: Text.ElideRight; width: purpleRectMapItem.width - 30;
                        text: ' c lat: ' + purpleRectMapItem.coordinate.latitude}
                    Text { color: 'yellow'; font.bold: true; style: Text.Outline; styleColor: 'black'; elide: Text.ElideRight; width: purpleRectMapItem.width - 30;
                        text: ' c lon: ' + purpleRectMapItem.coordinate.longitude}
                }
            }
        }

        /*
        MapQuickItem {
            z: 10
            objectName: "video item"
            coordinate: londonCoordinate
            sourceItem: Rectangle {
                color: "green"
                width: 200;
                height: 200;
                MediaPlayer {
                    id: player
                    source: "file:///home/juvuolle/Downloads/Big_Buck_Bunny_Trailer_400p.ogg.ogv"
                    playing: true
                }
                VideoOutput {
                    id: videoItem
                    source: player
                    anchors.fill: parent
                }
            }
        }
        */

/*
        MapQuickItem {
            z: 11
            objectName: "3d item"
            coordinate: londonCoordinate
            sourceItem: Item {
                width: 200
                height: 200
                Viewport {
                    width: 200
                    height: 200
                Item3D {
                    mesh: Mesh { source: "file:///home/juvuolle/qt/qt5/qtlocation/tests/applications/declarative_map/basket.bez" }
                    effect: Effect { texture: "file:///home/juvuolle/qt/qt5/qtlocation/tests/applications/declarative_map/basket.jpg" }
                    //! [2]
                    //! [3]

                    transform: [
                        Scale3D { scale: 1.5 },
                        Rotation3D {
                            axis: Qt.vector3d(0, 1, 0)
                            NumberAnimation on angle {
                                running: true
                                loops: Animation.Infinite
                                from: 0
                                to: 360
                                duration: 2000
                            }
                        }
                    ]
                }
                }
            }
        }
*/
        /*
        MapQuickItem {
            objectName: "blinky quick item 2"
            coordinate: brisbaneCoordinate
            anchorPoint: Qt.point(40, 40)
            zoomLevel: 6.0
            sourceItem: AnimatedImage {
                width: 80
                height: 80
                playing: true
                source: "blinky.gif"
                MouseArea {
                    anchors.fill: parent;
                    onClicked: {
                        parent.parent.zoomLevel += 1
                    }
                }
            }
        }
        */

        /*
        MapItem {
            objectName: 'blinky static item'
            zoomLevel: 7 // at which map's  zoom level the width and height are '1-to-1'
            coordinate: brisbaneCoordinate
            sourceItem: AnimatedImage {
                width: 80
                height: 80
                playing: true
                source: "blinky.gif"
            }
        }
        */


        MapItemView {
            id: theItemView
            model: testModel
            delegate: Component {
                MapQuickItem {
                    objectName: 'one of many items from model'
                    visible: true
                    zoomLevel: 7
                    sourceItem: Rectangle {
                        width: 300; height: 300; color: 'green'
                        Component.onCompleted: {
                            var num = (Math.floor(4 * Math.random()));
                            switch (num % 4) {
                            case 0:
                                color = "#ff0000";
                                break;
                            case 1:
                                color = "#0000ff";
                                break;
                            case 2:
                                color = "#00ffff";
                                break;
                            case 3:
                                color = "#00ff00";
                                break;
                            }
                        }

                    }
                    coordinate: Coordinate {
                        latitude: modeldata.coordinate.latitude;
                        longitude: modeldata.coordinate.longitude;
                    }
                }
            }
        }

        // From location.test plugin
        PinchGenerator {
            id: pinchGenerator
            anchors.fill: parent
            target: map
            enabled: false
            focus: true           // enables keyboard control for convenience
            replaySpeedFactor: 1.7 // replay with 1.1 times the recording speed to better see what happens
            Text {
                id: pinchGenText
                text: "PinchArea state: " + pinchGenerator.state + "\n"
                      + "Swipes recorded: " + pinchGenerator.count + "\n"
                      + "Replay speed factor: " + pinchGenerator.replaySpeedFactor
            }
        }

        Column {
            id: infoText
            y: 100
            spacing: 2
            Text {id: positionText; text: "Map zoom level: " + map.zoomLevel; color: 'red'; font.bold: true}
            Text {color: positionText.color; font.bold: true; width: page.width / 2; elide: Text.ElideRight; text: "Map center lat: " + mapCenterCoordinate.latitude }
            Text {color: positionText.color; font.bold: true; width: page.width / 2; elide: Text.ElideRight; text: "Map center lon: " + mapCenterCoordinate.longitude }
            Text {color: positionText.color; font.bold: true; width: page.width / 2; elide: Text.ElideRight; text: "Map bearing: " + map.bearing }
            Text {color: positionText.color; font.bold: true; width: page.width / 2; elide: Text.ElideRight; text: "Map tilt: " + map.tilt }
        }

        Grid {
            id: panNav
            z: 10
            anchors.top: infoText.bottom
            columns: 3
            spacing: 2
            Rectangle { id: navRect; width: 50; height: 50; color: 'peru'; Text {text: "\u2196";} MouseArea {anchors.fill: parent; onClicked: { map.pan(-5,5)}}}
            Rectangle { width: navRect.width; height: navRect.height; color: navRect.color; Text {text: "\u2191";} MouseArea {anchors.fill: parent; onClicked: {map.pan(0,5)}}}
            Rectangle { width: navRect.width; height: navRect.height; color: navRect.color; Text {text: "\u2197";} MouseArea {anchors.fill: parent; onClicked: {map.pan(5,5)}}}
            Rectangle { width: navRect.width; height: navRect.height; color: navRect.color; Text {text: "\u2190";} MouseArea {anchors.fill: parent; onClicked: {map.pan(-5,0)}}}
            Rectangle { width: navRect.width; height: navRect.height; color: navRect.color; Text {text: "Pan\nMap";} MouseArea {anchors.fill: parent; onClicked: {console.log('ticle tickle hehehe')}}}
            Rectangle { width: navRect.width; height: navRect.height; color: navRect.color; Text {text: "\u2192";} MouseArea {anchors.fill: parent; onClicked: {map.pan(5,0)}}}
            Rectangle { width: navRect.width; height: navRect.height; color: navRect.color; Text {text: "\u2199";} MouseArea {anchors.fill: parent; onClicked: {map.pan(-5,-5)}}}
            Rectangle { width: navRect.width; height: navRect.height; color: navRect.color; Text {text: "\u2193";} MouseArea {anchors.fill: parent; onClicked: {map.pan(0,-5)}}}
            Rectangle { width: navRect.width; height: navRect.height; color: navRect.color; Text {text: "\u2198";} MouseArea {anchors.fill: parent; onClicked: {map.pan(5,-5)}}}
        }

        Grid {
            id: itemMover
            z: 10
            anchors.top: panNav.bottom
            columns: 3
            spacing: 2
            property variant target: mapCircle

            Rectangle { id: itemMoverRect; width: 50; height: 50; color: 'peru'; Text {text: "\u2196";} MouseArea {anchors.fill: parent; onClicked: { itemMover.target.y -= 5; itemMover.target.x -= 5 }}}
            Rectangle { width: itemMoverRect.width; height: itemMoverRect.height; color: itemMoverRect.color; Text {text: "\u2191";} MouseArea {anchors.fill: parent; onClicked: {itemMover.target.y -= 5}}}
            Rectangle { width: itemMoverRect.width; height: itemMoverRect.height; color: itemMoverRect.color; Text {text: "\u2197";} MouseArea {anchors.fill: parent; onClicked: {itemMover.target.y -= 5; itemMover.target.x += 5}}}
            Rectangle { width: itemMoverRect.width; height: itemMoverRect.height; color: itemMoverRect.color; Text {text: "\u2190";} MouseArea {anchors.fill: parent; onClicked: {itemMover.target.x -= 5}}}
            Rectangle { width: itemMoverRect.width; height: itemMoverRect.height; color: itemMoverRect.color; Text {text: "Move\nCircle";} MouseArea {anchors.fill: parent; onClicked: {console.log('ticle tickle hehehe')}}}
            Rectangle { width: itemMoverRect.width; height: itemMoverRect.height; color: itemMoverRect.color; Text {text: "\u2192";} MouseArea {anchors.fill: parent; onClicked: {itemMover.target.x += 5}}}
            Rectangle { width: itemMoverRect.width; height: itemMoverRect.height; color: itemMoverRect.color; Text {text: "\u2199";} MouseArea {anchors.fill: parent; onClicked: {itemMover.target.x -= 5; itemMover.target.y += 5}}}
            Rectangle { width: itemMoverRect.width; height: itemMoverRect.height; color: itemMoverRect.color; Text {text: "\u2193";} MouseArea {anchors.fill: parent; onClicked: {itemMover.target.y += 5}}}
            Rectangle { width: itemMoverRect.width; height: itemMoverRect.height; color: itemMoverRect.color; Text {text: "\u2198";} MouseArea {anchors.fill: parent; onClicked: {itemMover.target.y += 5; itemMover.target.x += 5}}}
        }

        Rectangle {
            id: itemDataBackground
            opacity: 0.8
            anchors.top: itemMover.bottom
            anchors.bottom: rectangleData.bottom
            width: itemMover.width
        }
        Column {
            id: circleData;
            anchors.top: itemMover.bottom
            spacing: 1
            Text { text: ' MapCircle '; font.bold: true; font.pixelSize: 10; }
            Text { text: ' x: ' + mapCircle.x; font.pixelSize: 10; }
            Text { text: ' y: ' + mapCircle.y; font.pixelSize: 10; }
            Text { text: ' c lat: ' + mapCircle.center.latitude; font.pixelSize: 10; }
            Text { text: ' c lon: ' + mapCircle.center.longitude; font.pixelSize: 10; }
        }
        Column {
            id: polylineData;
            anchors.top: circleData.bottom
            spacing: 1
            Text { text: ' MapPolyline '; font.bold: true;font.pixelSize: 10; }
            Text { text: ' x: ' + mapPolyline.x; font.pixelSize: 10; }
            Text {text: ' y: ' + mapPolyline.y; font.pixelSize: 10; }
            Text {text: ' at(0) lat: ' + mapPolyline.path[0].latitude; font.pixelSize: 10; }
            Text {text: ' at(0) lon: ' + mapPolyline.path[0].longitude; font.pixelSize: 10; }
        }
        Column {
            id: polygonData;
            anchors.top: polylineData.bottom
            spacing: 1
            Text { text: ' MapPolygon '; font.bold: true;font.pixelSize: 10; }
            Text { text: ' x: ' + mapPolygon.x; font.pixelSize: 10; }
            Text {text: ' y: ' + mapPolygon.y; font.pixelSize: 10; }
            Text {text: ' at(0) lat: ' + mapPolygon.path[0].latitude; font.pixelSize: 10; }
            Text {text: ' at(0) lon: ' + mapPolygon.path[0].longitude; font.pixelSize: 10; }
        }
        Column {
            id: routeData;
            anchors.top: polygonData.bottom
            spacing: 1
            Text { text: ' MapRoute '; font.bold: true;font.pixelSize: 10; }
            Text { text: ' x: ' + mapRoute.x; font.pixelSize: 10; }
            Text {text: ' y: ' + mapRoute.y; font.pixelSize: 10; }
            Text {text: ' at(0) lat: ' + mapRoute.route.path[0].latitude; font.pixelSize: 10; }
            Text {text: ' at(0) lon: ' + mapRoute.route.path[0].longitude; font.pixelSize: 10; }
        }
        Column {
            id: quickItemData
            spacing: 1
            anchors.top: routeData.bottom
            Text {text: ' MapQuickItem ';font.bold: true; font.pixelSize: 10; }
            Text {text: ' x: ' + purpleRectMapItem.x; font.pixelSize: 10; }
            Text {text: ' y: ' + purpleRectMapItem.y; font.pixelSize: 10; }
            Text {text: ' tl lat: ' + purpleRectMapItem.coordinate.latitude; font.pixelSize: 10; }
            Text {text: ' tl lon: ' + purpleRectMapItem.coordinate.longitude; font.pixelSize: 10; }
        }
        Column {
            id: rectangleData
            spacing: 1
            anchors.top: quickItemData.bottom
            Text {text: ' MapRectangle '; font.bold: true; font.pixelSize: 10; }
            Text {text: ' x: ' + mapRectangle.x; font.pixelSize: 10; }
            Text {text: ' y: ' + mapRectangle.y; font.pixelSize: 10; }
            Text {text: ' tl lat: ' + mapRectangle.topLeft.latitude; font.pixelSize: 10; }
            Text {text: ' tl lon: ' + mapRectangle.topLeft.longitude; font.pixelSize: 10; }
            Text {text: ' br lat: ' + mapRectangle.bottomRight.latitude; font.pixelSize: 10; }
            Text {text: ' br lon: ' + mapRectangle.bottomRight.longitude; font.pixelSize: 10; }
        }



        /*
        Keys.onPressed: {
            if (event.key == Qt.Key_A) {
                console.log('Key A was pressed');
                //event.accepted = true;
            }
        }
        */

        Row {
            id: textRow1
            spacing: 15
            y: 1050
            Text {id: firstText; text: "Map zoom level: " + map.zoomLevel; color: 'red'; font.bold: true}
            Text {text: "Pinch zoom sensitivity: " + map.pinch.maximumZoomLevelChange; color: firstText.color; font.bold: true}
            Text {text: "Pinch rotation sensitivity: " + map.pinch.rotationFactor; color: firstText.color; font.bold: true}
        }

        Row {
            spacing: 15
            anchors.top: textRow1.bottom
            Text {text: "Flick deceleration: " + map.flick.deceleration; color: firstText.color; font.bold: true}
            Text {text: "Weather: Sunny, mild, late showers."; color: firstText.color; font.bold: true}
        }


        //plugin : Plugin {name : "nokia"}

        plugin: Plugin {
            id: testPlugin;
            name: "qmlgeo.test.plugin"
            parameters: [
                // Parms to guide the test plugin
                PluginParameter { name: "finishRequestImmediately"; value: true}
            ]
        }

        // commented features are checked to work at least somehow
        //anchors.left: parent.left
        //anchors.bottom: parent.bottom
        //anchors.leftMargin: 70
        //scale: 2
        //visible: false
        //transform: Translate {y: 200}
        //anchors.fill: page
        width: page.width - 80
        height: 1000
        zoomLevel: 9

        // pinch.activeGestures: MapPinchArea.ZoomGesture | RotationGesture
        pinch.activeGestures: MapPinchArea.NoGesture
        pinch.enabled: true
        pinch.maximumZoomLevelChange: 4.0 // maximum zoomlevel changes per pinch
        pinch.rotationFactor: 1.0 // default ~follows angle between fingers

        // Flicking
        flick.enabled: true
        // flick.deceleration: 500
        //flick.onFlickStarted: {console.log ('flick started signal                    F Start ++++++++++++++++++ ') }
        //flick.onFlickEnded: {console.log ('flick ended signal                        F Stop ------------------ ') }
        //flick.onMovementStarted: {console.log('movement started signal                   M Start ++++++++++++++++++ ') }

        //flick.onMovementEnded: {console.log ('movement ended signal                     M Stop ------------------ ') }

        onWheel: {
            //console.log('map wheel event, rotation in degrees: ' + delta/8);
            if (delta > 0) map.zoomLevel += 0.25
            else map.zoomLevel -= 0.25
        }

        pinch.onPinchStarted: {
            console.log('Map element pinch started---------+++++++++++++++++++++++++++++++++++++')
            pinchRect1.x = pinch.point1.x; pinchRect1.y = pinch.point1.y;
            pinchRect2.x = pinch.point2.x; pinchRect2.y = pinch.point2.y;
            pinchRect1.visible = true; pinchRect2.visible = true;
            console.log('Center : ' + pinch.center)
            console.log('Angle: ' + pinch.angle)
            console.log('Point count: ' + pinch.pointCount)
            console.log('Accepted: ' + pinch.accepted)
            console.log('Point 1: ' + pinch.point1)
            console.log('Point 2: ' + pinch.point2)
        }
        pinch.onPinchUpdated: {
            console.log('Map element pinch updated---------+++++++++++++++++++++++++++++++++++++')
            pinchRect1.x = pinch.point1.x; pinchRect1.y = pinch.point1.y;
            pinchRect2.x = pinch.point2.x; pinchRect2.y = pinch.point2.y;
            console.log('Center : ' + pinch.center)
            console.log('Angle: ' + pinch.angle)
            console.log('Point count: ' + pinch.pointCount)
            console.log('Accepted: ' + pinch.accepted)
            console.log('Point 1: ' + pinch.point1)
            console.log('Point 2: ' + pinch.point2)
        }
        pinch.onPinchFinished: {
            console.log('Map element pinch finished ---------+++++++++++++++++++++++++++++++++++++')
            pinchRect1.visible = false; pinchRect2.visible = false;
            console.log('Center : ' + pinch.center)
            console.log('Angle: ' + pinch.angle)
            console.log('Point count: ' + pinch.pointCount)
            console.log('Accepted: ' + pinch.accepted)
            console.log('Point 1: ' + pinch.point1)
            console.log('Point 2: ' + pinch.point2)
        }

        center: Coordinate {
            id: mapCenterCoordinate
            latitude: 51.5
            longitude: -0.11
        }

        // <unsupported so far>
        //rotation: 10 // strangely impacts the size of the map element though
        //transform: Scale { origin.x: 25; origin.y: 25; xScale: 3}          // weirdly translates the item
        //transform: Rotation { origin.y: 25; origin.x: 25; angle: 45}       // weirdly translates the item
        //z: 4 // map will always be under everything, will not be supported
        //opacity: 0.4 // doesn't probably make sense
        //clip: true // not implemented, not sure if very useful either
        // </unsupported so far>
    }

    Plugin {id: nokia_plugin; name: "nokia"}

    Row {
        id: buttonRow
        anchors.leftMargin: 2
        anchors.topMargin: 2
        anchors.top: map.bottom;
        spacing: 2
        Rectangle { id: rowRect1; width: 80; height: 45; color: 'peru';
            MouseArea { anchors.fill: parent; onClicked: { map.pinch.maximumZoomLevelChange += 0.1}
                Text {text: "Pinch zoom\nsensitivity+"}
            }
        }
        Rectangle { width: rowRect1.width; height: rowRect1.height; color: rowRect1.color;
            MouseArea { anchors.fill: parent; onClicked: {map.pinch.maximumZoomLevelChange -= 0.1}
                Text {text: "Pinch zoom\nsensitivity-"}
            }
        }
        Rectangle { width: rowRect1.width; height: rowRect1.height; color: rowRect1.color;
            MouseArea { anchors.fill: parent; onClicked: {map.pinch.rotationFactor += 0.1}
                Text {text: "Pinch rotation\nsensitivity+"}
            }
        }
        Rectangle { width: rowRect1.width; height: rowRect1.height; color: rowRect1.color;
            MouseArea { anchors.fill: parent; onClicked: {map.pinch.rotationFactor -= 0.1}
                Text {text: "Pinch rotation\nsensitivity-"}
            }
        }
        Rectangle { id: rowRectPinchGen; width: rowRect1.width; height: rowRect1.height; color: 'lightsteelblue';
            Text { text: "Pinch\nzoom:\n"  + ((map.pinch.activeGestures & MapPinchArea.ZoomGesture) > 0? "Yes":"No")}
            MouseArea {
                anchors.fill: parent;
                onClicked: {
                    console.log('map pinch active gestures' + map.pinch.activeGestures);
                    if (map.pinch.activeGestures & MapPinchArea.ZoomGesture)
                        map.pinch.activeGestures &= ~MapPinchArea.ZoomGesture
                    else
                        map.pinch.activeGestures += MapPinchArea.ZoomGesture
                }
            }
        }
        Rectangle { width: rowRect1.width; height: rowRect1.height; color: rowRectPinchGen.color;
            Text { text: "Pinch\nrotation:\n"  + ((map.pinch.activeGestures & MapPinchArea.RotationGesture) > 0? "Yes":"No")}
            MouseArea {
                anchors.fill: parent;
                onClicked: {
                    console.log('map pinch active gestures' + map.pinch.activeGestures);
                    if (map.pinch.activeGestures & MapPinchArea.RotationGesture)
                        map.pinch.activeGestures &= ~MapPinchArea.RotationGesture
                    else
                        map.pinch.activeGestures += MapPinchArea.RotationGesture
                }
            }
        }

        Rectangle { width: rowRect1.width; height: rowRect1.height; color: rowRectPinchGen.color;
            Text { text: "Pinch\ntilt:\n"  + ((map.pinch.activeGestures & MapPinchArea.TiltGesture) > 0? "Yes":"No")}
            MouseArea {
                anchors.fill: parent;
                onClicked: {
                    console.log('map pinch active gestures' + map.pinch.activeGestures);
                    if (map.pinch.activeGestures & MapPinchArea.TiltGesture)
                        map.pinch.activeGestures &= ~MapPinchArea.TiltGesture
                    else
                        map.pinch.activeGestures += MapPinchArea.TiltGesture
                }
            }

        }
        Rectangle { width: rowRect1.width; height: rowRect1.height; color: rowRectPinchGen.color;
            Text { id: generatorEnabledText; text: pinchGenerator.enabled? "Pinch Gen\nEnabled" : "Pinch Gen\nDisabled"; font.bold: true}
            MouseArea {
                anchors.fill: parent;
                onClicked: {
                    if (pinchGenerator.focus == true) {
                        pinchGenerator.focus = false;
                        pinchGenerator.enabled = false;
                        pinchGenerator.z = -1
                        map.focus = true;
                    } else {
                        pinchGenerator.focus = true
                        pinchGenerator.enabled = true;
                        pinchGenerator.z = 10
                        map.focus = false
                    }
                }
            }
        }
        Rectangle { width: rowRect1.width; height: rowRect1.height; color: rowRect1.color;
            Text {text: map.flick.enabled? "Flick\nEnabled":"Flick\nDisabled"; font.bold: true}
            MouseArea { anchors.fill: parent; onClicked: {map.flick.enabled = !map.flick.enabled} }
        }
        Rectangle { width: rowRect1.width; height: rowRect1.height; color: rowRect1.color;
            MouseArea { anchors.fill: parent; onClicked: {map.flick.deceleration += 200}
                Text {text: "Flick\ndeceleration+"}
            }
        }
        Rectangle { width: rowRect1.width; height: rowRect1.height; color: rowRect1.color;
            MouseArea { anchors.fill: parent; onClicked: {map.flick.deceleration -= 200}
                Text {text: "Flick\ndeceleration-"}
            }
        }
    } // Row

    Rectangle {
        id: pinchRect1
        color: 'red'
        visible: false
        z: 10
        width: 5
        height: 5
    }
    Rectangle {
        id: pinchRect2
        color: 'red'
        visible: false
        z: 10
        width: 5
        height: 5
    }

    Repeater {
        id: swipeView1
        model: pinchGenerator.swipe1
        delegate: Component {
            Rectangle {
                Text {id: touchPointText}
                Component.onCompleted: {
                    if (modelData.touchState == 1)  {    // Qt.TouchPointPressed
                        color = "pink"; width = 15; height = 15
                        touchPointText.text = 'From'
                    }
                    else if (modelData.touchState == 2) { // Qt.TouchPointMoved
                        color = 'yellow'; width = 5; height = 5
                    }
                    else if (modelData.touchState == 8) { // Qt.TouchPointReleased
                        color = 'red'; width = 15; height = 15
                        touchPointText.text = 'To'
                    }
                }
                x: modelData.targetX; y: modelData.targetY
            }
        }
    }

    Repeater {
        id: swipeView2
        model: pinchGenerator.swipe2
        delegate: Component {
                Rectangle {
                Text {id: touchPoint2Text}
                Component.onCompleted: {
                    if (modelData.touchState == 1)  {    // Qt.TouchPointPressed
                        color = "green"; width = 15; height = 15
                        touchPoint2Text.text = 'From'
                    }
                    else if (modelData.touchState == 2) { // Qt.TouchPointMoved
                        color = 'yellow'; width = 5; height = 5
                    }
                    else if (modelData.touchState == 8) { // Qt.TouchPointReleased
                        color = 'blue'; width = 15; height = 15
                        touchPoint2Text.text = 'To'
                    }
                }
                x: modelData.targetX; y: modelData.targetY
            }
        }
    }
}
