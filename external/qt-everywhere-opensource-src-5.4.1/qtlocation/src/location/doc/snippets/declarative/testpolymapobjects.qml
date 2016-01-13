/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Mobility Components.
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

import Qt 4.7
import QtMobility.location 1.2
import "landmarkmapcommon" as Common
import "landmarkmapmobile" as Mobile

import QtQuick 1.0

Item {
    id: page
    width: 420
    height: 580
    focus: true


    //![Wherever I may roam]
    PositionSource {
        id: myPositionSource
        active: true
        nmeaSource: "nmealog.txt"
        updateInterval: 2000
        onPositionChanged: {
            console.log("Position changed in PositionSource")
            myPathPolyline.addCoordinate(position.coordinate)
        }
    }
    //![Wherever I may roam]

    LandmarkModel {
        id: landmarkModel
        autoUpdate: true
        onModelChanged: {
            console.log("Landmark model changed, landmark count: " + count)
        }
        limit: 20
    }

    Mobile.TitleBar { id: titleBar; z: 5; width: parent.width - statusBar.width; height: 40; opacity: 0.8 }
    Mobile.StatusBar { id: statusBar; z: 6; width: 80; height: titleBar.height; opacity: titleBar.opacity; anchors.right: parent.right}

    Coordinate {
        id: defaultMapCenter
        latitude: -28.35
        longitude: 153.4
    }

    Rectangle {
        id: dataArea
        anchors.top: titleBar.bottom
        anchors.bottom: page.bottom
        width: page.width
        color: "#343434"
        Image { source: "landmarkmapmobile/images/stripes.png"; fillMode: Image.Tile; anchors.fill: parent; opacity: 0.3 }

        //![MapObjectView]
        Map {
            id: map
            plugin : Plugin { name : "nokia" }
            anchors.fill: parent; size.width: parent.width; size.height: parent.height; zoomLevel: 12
            center: myPositionSource.position.coordinate

            MapObjectView {
                id: circle_basic_view
                model: landmarkModel
                    delegate: Component {
                        id: circleMapDelegate
                        MapCircle {
                            color: "red"
                            radius: 500
                            center: landmark.coordinate
                        }
                    }
            }
            //![MapObjectView]

            MapObjectView {
                id: text_basic_view
                model: landmarkModel
                delegate: textMapDelegate
            }

            MapPolyline {
                id: myPathPolyline
                border {color: "red"; width: 4}
            }

            //![Basic MapPolygon]
            MapPolygon {
                id: polygon
                color: "blue"
                Coordinate {
                    id: topLeftCoordinate
                    latitude: -28.35
                    longitude: 153.4
                }
                Coordinate {
                    id: rightCoordinate
                    latitude: -28.34
                    longitude: 153.45
                }
                Coordinate {
                    id: bottomLeftCoordinate
                    latitude: -28.33
                    longitude: 153.4
                }
            }
            //![Basic MapPolygon]
            //![Basic MapPolyline]
            MapPolyline {
                id: polyline
                border {color: "red"; width: 4}
                Coordinate {
                    id: polylineTopLeftCoordinate
                    latitude: -28.35
                    longitude: 153.4
                }
                Coordinate {
                    id: polylineRightCoordinate
                    latitude: -28.34
                    longitude: 153.45
                }
                Coordinate {
                    id: polylineBottomLeftCoordinate
                    latitude: -28.33
                    longitude: 153.4
                }
                onPathChanged: {
                    console.log('Polyline pathChanged signal received')
                }
            }
            //![Basic MapPolyline]

            //![Basic map position marker definition]
            MapCircle {
                id: myPositionMarker
                center: myPositionSource.position.coordinate
                radius: 100
                color: "yellow"
            }
            //![Basic map position marker definition]
        }

        MouseArea {
            anchors.fill: parent

            property bool mouseDown : false
            property int lastX : -1
            property int lastY : -1

            onPressed : {
                mouseDown = true
                // While panning, its better not to actively udpate the model
                // as it results in poor performance. Instead set opacity to make
                // it more obvious that the landmark positions are not valid.
                landmarkModel.autoUpdate = false
                lastX = mouse.x
                lastY = mouse.y
            }
            onReleased : {
                mouseDown = false
                landmarkModel.autoUpdate = true
                landmarkModel.update()
                lastX = -1
                lastY = -1
            }
            onPositionChanged: {
                if (mouseDown) {
                    var dx = mouse.x - lastX
                    var dy = mouse.y - lastY
                    map.pan(-dx, -dy)
                    page.state = "NoFollowing"
                    lastX = mouse.x
                    lastY = mouse.y
                }
            }
            onDoubleClicked: {
                page.state = "NoFollowing"
                map.center = map.toCoordinate(Qt.point(mouse.x, mouse.y))
                if (map.zoomLevel < map.maximumZoomLevel)
                    map.zoomLevel += 1
            }
        }

        Component {
            id: textMapDelegate
            //![MapText]
            MapText {
                color: "blue"
                coordinate: landmark.coordinate
                text: landmark.name
                font.pointSize: 8
            }
            //![MapText]
        }

        Component {
            id: rectangleMapDelegate
            //![MapRectangle]
            MapRectangle {
                color: "yellow"
                topLeft: landmark.coordinate
                bottomRight: Coordinate {
                    latitude: landmark.coordinate.latitude - 1
                    longitude: landmark.coordinate.longitude + 1
                }
            }
            //![MapRectangle]
        }

        /* map mouse area testing
        Component {
            id: circleMapDelegate
            MapCircle {
                color: "red"
                radius: 500
                center: landmark.coordinate

                MapMouseArea {
                    onPressed : {
                        console.log('pressed in model circle')
                        parent.color = "red"
                    }
                    onReleased : {
                        console.log('released in model circle')
                        parent.color = "yellow"
                    }
                }
            }
        }
        */

        Mobile.Floater {
            id : dataFloater
            latitude: myPositionSource.position.coordinate.latitude
            longitude:  myPositionSource.position.coordinate.longitude
            landmarks: landmarkModel.count
        }

        Item {
            id: sliderContainer
            anchors {bottom: toolbar0.top;}
            height:  40
            width: parent.width

            Common.Slider {
                id: zoomSlider;
                minimum: 1; maximum: 18;
                anchors {
                    fill: parent
                    bottomMargin: 5; rightMargin: 5; leftMargin: 5
                }
                onValueChanged: {
                    map.zoomLevel = value
                }
            }
        }

        Mobile.ToolBar {
            id: toolbar0
            opacity: titleBar.opacity
            height: 40; width: parent.width
            anchors.bottom: toolbar1.top
            z: 6
            button1Label: "-myposmark"; button2Label: "+myposmark"; button3Label: "-"
            //![Basic remove MapObject]
            onButton1Clicked: {
                map.removeMapObject(myPositionMarker)
            }
            //![Basic remove MapObject]
            //![Basic add MapObject]
            onButton2Clicked: {
                map.addMapObject(myPositionMarker)
            }
            //![Basic add MapObject]
            onButton3Clicked: {
            }
        }

        Mobile.ToolBar {
            id: toolbar1
            opacity: titleBar.opacity
            height: 40; width: parent.width
            anchors.bottom: toolbar2.top
            z: 6
            button1Label: "+polyg"; button2Label: "-polyg"; button3Label: "-initpolyg3"
            //![Adding to polygon]
            onButton1Clicked: {
                polygon.addCoordinate(map.center)
            }
            //![Adding to polygon]
            //![Removing from polygon]
            onButton2Clicked: {
                polygon.removeCoordinate(map.center)
            }
            //![Removing from polygon]
            onButton3Clicked: {
                polygon.removeCoordinate(bottomLeftCoordinate)
            }
        }

        Mobile.ToolBar {
            id: toolbar2
            opacity: titleBar.opacity
            height: 40; width: parent.width
            anchors.bottom: toolbar3.top
            z: 6
            button1Label: "-iterpolyg2nd"; button2Label: "no follow"; button3Label: "+initpolyg3"
            //![Iterating and removing polygon]
            onButton1Clicked: {
                for (var index = 0; index < polygon.path.length; index++)  {
                    console.log("Latitude at index:" + index + " , " + polygon.path[index].latitude);
                }
                polygon.removeCoordinate(polygon.path[2])
            }
            //![Iterating and removing polygon]
            onButton2Clicked: {
                map.center = defaultMapCenter
                myPositionSource.active = false
            }
            onButton3Clicked: {
                polygon.addCoordinate(bottomLeftCoordinate)
            }
        }

        Mobile.ToolBar {
            id: toolbar3
            opacity: toolbar1.opacity
            height: 40; width: parent.width
            anchors.bottom: toolbar4.top
            z: 6
            button1Label: "+polyl"; button2Label: "-polyl"; button3Label: "+initpolyl3"
            onButton1Clicked: {
                polyline.addCoordinate(map.center)
            }
            //![Removing from polyline]
            onButton2Clicked: {
                polyline.removeCoordinate(map.center)
            }
            //![Removing from polyline]
            onButton3Clicked: {
                polyline.addCoordinate(polylineBottomLeftCoordinate)
            }
        }

        Mobile.ToolBar {
            id: toolbar4
            opacity: toolbar1.opacity
            height: 40; width: parent.width
            anchors.bottom: parent.bottom
            z: 6
            button1Label: "-iterpolyl2nd"; button2Label: "-"; button3Label: "-initpolyl3"
            //![Iterating and removing polyline]
            onButton1Clicked: {
                for (var index = 0; index < polyline.path.length; index++)  {
                    console.log("Index, latitude:" + index + " , " + polyline.path[index].latitude);
                }
                polyline.removeCoordinate(polyline.path[2])
            }
            //![Iterating and removing polyline]
            onButton2Clicked: {
            }
            onButton3Clicked: {
                polyline.removeCoordinate(polylineBottomLeftCoordinate)
            }
        }

    } // dataArea

} // page



