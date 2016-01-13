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

import QtQuick 2.0
import QtPositioning 5.2
import QtLocation 5.3
import QtLocation.test 5.0
import "common" as Common

Item {
    objectName: "The page."
    width:  1140     //360
    height: 1085    // 640
    //width:  360
    //height: 640
    id: page

    //Rectangle {
    //    id: bottleAnimation
    //    width: animation.width; height: animation.height + 8
        // }

    // From location.test plugin
    TestModel {
          id: testModel
          datatype: 'coordinate'
          datacount: 8
          delay: 0
          crazyMode:  false  // generate arbitrarily updates. interval is set below, and the number of items is varied between 0..datacount
          crazyLevel: 2000 // the update interval varies between 3...crazyLevel (ms)
      }

    Item {
        visible: false
        id: shaderItem
        width: 256
        height: 256
        x: 100
        y: 100
        Rectangle {
            radius: 20
            id: shaderRect
            width: parent.width
            height: parent.height/4
            color: 'red'
            z: 1
            Text {text: "Wicked!"}

            SequentialAnimation on color {
                loops: Animation.Infinite
                ColorAnimation { from: "DeepSkyBlue"; to: "red"; duration: 2000 }
                ColorAnimation { from: "red"; to: "DeepSkyBlue"; duration: 2000 }
            }
        }
        Rectangle {
            id: shaderRect2
            opacity: 0.5
            //width: parent.width
            anchors.fill: shaderRect
            //height: parent.height/4
            color: 'green'
            Text {text: "Sick!"}
        }
        Rectangle {
            id: shaderRect3
            width: parent.width
            anchors.top: shaderRect2.bottom
            height: parent.height/4
            color: 'blue'
            Text {text: "Sick!"}
        }
        Rectangle {
            width: parent.width
            anchors.top: shaderRect3.bottom
            height: parent.height/4
            color: 'yellow'
            Text {text: "Sick!"}
        }
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
                onClicked: {console.log('oooQML: ----------------adding item 1'); map.addMapItem(externalStaticMapItem1);}
                onDoubleClicked: {console.log('oooQML: +++++++++++++++ removing item 1'); map.removeMapItem(externalStaticMapItem1);}
            }
        }
        Rectangle {color: "lightblue"; width: 80; height: 80;
            Text {text: "Click:\nadd item2\nDouble-click:\nrm item2"}
            MouseArea{ anchors.fill: parent;
                onClicked: {console.log('oooQML: adding item 2'); map.addMapItem(externalStaticMapItem2);}
                onDoubleClicked: {console.log('oooQML: removing item 2'); map.removeMapItem(externalStaticMapItem2);}
            }
        }
    }

    /*
    MapItem {
        id: externalStaticMapItem1
        objectName: "externalStaticMapItem1"
        coordinate: brisbaneCoordinate
        zoomLevel: 5.0
        source: Rectangle {
            color: "gray"
            width: 140
            height: 20
            Text {font.pixelSize: 15;text: "ext map item 1"; font.bold: true; color: 'red'}
        }
    }

    MapItem {
        id: externalStaticMapItem2
        objectName: "externalStaticMapItem2"
        coordinate: brisbaneCoordinate2
        zoomLevel: 5.0
        source: Rectangle {
            color: "gray"
            width: 140
            height: 20
            Text {font.pixelSize: 15;text: "ext map item 2"; font.bold: true; color: 'red'}
        }
    }
    */

    /*
    MapItem {
        id: mapItem1
        source: AnimatedImage {width: 80; height: 80; playing: true; source: "walk.gif"}
    }
    */

    //AnimatedImage {width: 80; height: 80; playing: true; source: "walk.gif"}
    //MapItem {id: mapItem2 }
    //MapItem {id: mapItem3 }
    //MapItem {id: mapItem4 }
    //MapItem {id: mapItem5 }
    //MapItem {id: mapItem6 }
    //MapItem {id: mapItem7 }
    //MapItem {id: mapItem8 }

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

    Map {
        id: map
        /*
        MapItem {
            objectName: 'blinky static item'
            zoomLevel: 7 // at which map's  zoom level the width and height are '1-to-1'
            coordinate: brisbaneCoordinate
            source: AnimatedImage {
                width: 80
                height: 80
                playing: true
                source: "blinky.gif"
            }
        }
        */


        //MapItem {
        //    source: Rectangle { width: 40; height: 40; color: 'chocolate'
        //    }
        // }
        /*
        MapObjectView {
            id: theObjectView
            model: testModel
            delegate: Component {
                MapItem {
                    objectName: 'one of many items from model'
                    visible: true
                    live: true
                    recursive: true
                    source: Rectangle {
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
        */

        // From location.test plugin
        PinchGenerator {
            id: pinchGenerator
            anchors.fill: parent
            target: map
            enabled: false
            focus: true           // enables keyboard control for convenience
            replaySpeedFactor: 1.1 // replay with 1.1 times the recording speed to better see what happens
            Text {
                text: "PinchArea state: " + pinchGenerator.state + "\n"
                      + "Swipes recorded: " + pinchGenerator.count + "\n"
                      + "Replay speed factor: " + pinchGenerator.replaySpeedFactor
            }
        }
        /*
        Keys.onPressed: {
            if (event.key == Qt.Key_A) {
                console.log('oooQML: Key A was pressed');
                //event.accepted = true;
            }
        }
        */
        plugin : Plugin {name : "nokia"}
        // commented features are checked to work at least somehow
        x: 0
        y: 0
        //size.width: 100
        //size.height: 100
        //anchors.left: parent.left
        //anchors.bottom: parent.bottom
        //anchors.leftMargin: 70
        //scale: 2
        //visible: false
        //transform: Translate {y: 200}
        //anchors.fill: page
        width: page.width - 80
        height: 800
        zoomLevel: 5.1

        // pinch.activeGestures: MapPinchArea.ZoomGesture | RotationGesture
        pinch.activeGestures: MapPinchArea.NoGesture

        pinch.enabled: true

        // Flicking
        flick.enabled: true
        flick.deceleration: 3000
        flick.onFlickStarted: {console.log   ('flick started signal                      F Start ++++++++++++++++++ ') }
        flick.onFlickEnded: {console.log     ('flick ended signal                        F Stop ------------------ ') }
        flick.onMovementStarted: {console.log('oooQML: movement started signal                   M Start ++++++++++++++++++ ') }
        flick.onMovementEnded: {console.log  ('movement ended signal                     M Stop ------------------ ') }


        onWheel: {
            console.log('oooQML: map wheel event, rotation in degrees: ' + delta/8);
            if (delta > 0) map.zoomLevel += 0.25
            else map.zoomLevel -= 0.25
        }

        pinch.onPinchStarted: {
            console.log('oooQML: Map element pinch started---------+++++++++++++++++++++++++++++++++++++')
            pinchRect1.x = pinch.point1.x; pinchRect1.y = pinch.point1.y;
            pinchRect2.x = pinch.point2.x; pinchRect2.y = pinch.point2.y;
            pinchRect1.visible = true; pinchRect2.visible = true;
            //console.log('oooQML: Point 1 x: ' + pinch.point1.x + ' Point2 x' + pinch.point2.x)
            //console.log('oooQML: Center x: ' + pinch.center.x + ' Point1 y: ' + pinch.point1.y)
        }
        pinch.onPinchUpdated: {
            console.log('oooQML: Map element pinch updated---------+++++++++++++++++++++++++++++++++++++')
            pinchRect1.x = pinch.point1.x; pinchRect1.y = pinch.point1.y;
            pinchRect2.x = pinch.point2.x; pinchRect2.y = pinch.point2.y;
        }
        pinch.onPinchFinished: {
            console.log('oooQML: Map element pinch finished ---------+++++++++++++++++++++++++++++++++++++')
            pinchRect1.visible = false; pinchRect2.visible = false;
            //map.pinch.minimumZoomLevel =  map.zoomLevel - 2
            //map.pinch.maximumZoomLevel = map.zoomLevel + 2
        }


        Rectangle {
            id: mouseRectUpper
            color: 'green'
            border.color: "yellow"
            border.width: 5
            radius: 10
            opacity: 0.2
            x: 0; y: 0;
            width: map.width;
            height: map.height / 2
            Text { font.pixelSize: 20; text: '\n\n\n\n\n\n     upper MapMouseArea, z value: ' + mouseAreaOfMap.z}
        }

        MapMouseArea {
            id: mouseAreaOfMap
            objectName: 'map mouse area'
            z: 5
            x: 0; y: 0;
            width: map.width;
            height: map.height / 2;

            onAcceptedButtonsChanged: {
               console.log('oooQML: in QML MapMouseArea acceptedButtonsChanged: ' + mouseAreaOfMap.acceptedButtons + ' ' + acceptedButtons)
            }
            onEnabledChanged: {
                console.log('oooQML: in QML MapMouseArea enabledChanged: ' + mouseAreaOfMap.enabled)
            }
            onPressed: {
                console.log('oooQML: in QML MapMouseArea pressed: ' +
                            mouse.x + ' y:' + mouse.y + ' pressed: '
                            + pressed + ' mouseX: ' + mouseAreaOfMap.mouseX + ' mouseY: ' + mouseAreaOfMap.mouseY + ' button: ' + mouse.button + ' buttons: ' + mouse.buttons)
            }
            onPressedChanged: {
                console.log('oooQML: in QML MapMouseArea pressedChanged, pressedButtons: ' + mouseAreaOfMap.pressedButtons)
            }
            onPressedButtonsChanged: {
                console.log('oooQML: in QML MapMouseArea pressedButtonChanged ' + ' pressed button: ' + mouseAreaOfMap.pressedButtons)
            }
            onReleased: {
                console.log('oooQML: in QML MapMouseArea released: ' + mouse.x + ' y: ' + mouse.y)
            }
            onDoubleClicked: {
                console.log('oooQML: in QML MapMouseArea doubleclicked---------------------------------------- dump event: ')
                console.log('oooQML: accepted: ' + mouse.accepted);
                console.log('oooQML: button: ' + mouse.button);
                console.log('oooQML: modifiers: ' + mouse.modifiers);
                console.log('oooQML: wasHeld: ' + mouse.wasHeld);
                console.log('oooQML: x: ' + mouse.x);
                console.log('oooQML: y: ' + mouse.y);
                console.log('oooQML: mouse area x,y, width, height: ' + mouseAreaOfMap.x + ' ' + mouseAreaOfMap.y + ' ' + mouseAreaOfMap.width + ' ' + mouseAreaOfMap.height)

                console.log('oooQML: in QML MapMouseArea doubleclicked---------------------------------------- end dump ')
            }
            onClicked: {
                    console.log('oooQML: in QML MapMouseArea clicked')
            }
            onPositionChanged: {
                   console.log('oooQML: in QML MapMouseArea position changed, x: ' + mouse.x + ' y: ' + mouse.y
                               + ' mouseX: ' + mouseAreaOfMap.mouseX + ' mouseY: ' + mouseAreaOfMap.mouseY + ' button: ' + mouse.button + ' buttons: ' + mouse.buttons)
            }
            onEntered: {
                console.log('oooQML: in QML MapMouseArea entered')
            }
            onExited: {
                console.log('oooQML: in QML MapMouseArea exited')
            }
            onPressAndHold: {
                console.log('oooQML: in QML MapMouseArea press and hold')
            }
            // This signal is not officially public per se, but used to notify containsMouse changes
            onHoveredChanged: {
                console.log('oooQML: in QML MapMouseArea hoveredChanged, containsMouse is: ' + mouseAreaOfMap.containsMouse)
            }
        }

        Rectangle {
            id: mouseRectLower
            color: 'green'
            border.color: "yellow"
            border.width: 5
            radius: 10
            opacity: 0.2
            x: 0; y: map.height/2;
            width: map.width;
            height: map.height/2;
            Text { font.pixelSize: 20; text: '                                          lower MapMouseArea, z value: ' + mouseAreaOfMap2.z}
        }

        MapMouseArea {
            id: mouseAreaOfMap2
            objectName: 'map mouse area 2'
            x: 0; y: map.height/2;
            width: map.width;
            height: map.height/2;

            hoverEnabled: true

            onAcceptedButtonsChanged: {
               console.log('oooQML: in QML MapMouseArea2 acceptedButtonsChanged: ' + mouseAreaOfMap2.acceptedButtons + ' ' + acceptedButtons)
            }
            onEnabledChanged: {
                console.log('oooQML: in QML MapMouseArea2 enabledChanged: ' + mouseAreaOfMap2.enabled)
            }
            onPressed: {
                console.log('oooQML: in QML MapMouseArea2 pressed: ' +
                            mouse.x + ' y:' + mouse.y + ' pressed: '
                            + pressed + ' mouseX: ' + mouseAreaOfMap2.mouseX + ' mouseY: ' + mouseAreaOfMap2.mouseY)
                console.log('oooQML: and the geo coordinate for that is, lat: '
                            + mouseAreaOfMap2.mouseToCoordinate(mouse).latitude + ' + lon : ' +
                            mouseAreaOfMap2.mouseToCoordinate(mouse).longitude)
            }
            onPressedChanged: {
                console.log('oooQML: in QML MapMouseArea2 pressedChanged, pressedButtons: ' + mouseAreaOfMap2.pressedButtons)
            }
            onPressedButtonsChanged: {
                console.log('oooQML: in QML MapMouseArea2 pressedButtonChanged ' + ' pressed button: ' + mouseAreaOfMap2.pressedButtons)
            }
            onReleased: {
                console.log('oooQML: in QML MapMouseArea2 released: ' + mouse.x + ' y: ' + mouse.y)
            }
            onDoubleClicked: {
                console.log('oooQML: in QML MapMouseArea2 doubleclicked---------------------------------------- dump event: ')
                console.log('oooQML: accepted: ' + mouse.accepted);
                console.log('oooQML: button: ' + mouse.button);
                console.log('oooQML: modifiers: ' + mouse.modifiers);
                console.log('oooQML: wasHeld: ' + mouse.wasHeld);
                console.log('oooQML: x: ' + mouse.x);
                console.log('oooQML: y: ' + mouse.y);
                console.log('oooQML: mouse area x,y, width, height: ' + mouseAreaOfMap2.x + ' ' + mouseAreaOfMap2.y + ' ' + mouseAreaOfMap2.width + ' ' + mouseAreaOfMap2.height)

                console.log('oooQML: in QML MapMouseArea2 doubleclicked---------------------------------------- end dump ')
            }
            onClicked: {
                    console.log('oooQML: in QML MapMouseArea2 clicked')
            }
            onPositionChanged: {
                   console.log('oooQML: in QML MapMouseArea2 position changed, x: ' + mouse.x + ' y: ' + mouse.y
                               + ' mouseX: ' + mouseAreaOfMap2.mouseX + ' mouseY: ' + mouseAreaOfMap2.mouseY)
            }
            onEntered: {
                console.log('oooQML: in QML MapMouseArea2 entered')
            }
            onExited: {
                console.log('oooQML: in QML MapMouseArea2 exited')
            }
            onPressAndHold: {
                console.log('oooQML: in QML MapMouseArea2 press and hold')
            }
            // This signal is not officially public per se, but used to notify containsMouse changes
            onHoveredChanged: {
                console.log('oooQML: in QML MapMouseArea2 hoveredChanged, containsMouse is: ' + mouseAreaOfMap2.containsMouse)
            }
        }

        // overlaps both mouse areas 1 and 2 (rhs)
        Rectangle {
            id: mouseRectRightOverlapping
            color: 'green'
            border.color: "red"
            border.width: 5
            radius: 10
            opacity: 0.2
            x: map.width/2; y: 0;
            width: map.width/2;
            height: map.height;
            Text { font.pixelSize: 20; text: '\n\n\n\n\n overlapping MapMouseArea (3), Z value: ' + mouseAreaOfMap3.z}
        }

        MapMouseArea {
            id: mouseAreaOfMap3
            objectName: 'map mouse area 3'
            z: 4
            x: map.width/2; y: 0;
            width: map.width/2;
            height: map.height;

            onAcceptedButtonsChanged: {
               console.log('oooQML: in QML MapMouseArea3 acceptedButtonsChanged: ' + mouseAreaOfMap3.acceptedButtons + ' ' + acceptedButtons)
            }
            onEnabledChanged: {
                console.log('oooQML: in QML MapMouseArea3 enabledChanged: ' + mouseAreaOfMap3.enabled)
            }
            onPressed: {
                console.log('oooQML: in QML MapMouseArea3 pressed: ' +
                            mouse.x + ' y:' + mouse.y + ' pressed: '
                            + pressed + ' mouseX: ' + mouseAreaOfMap3.mouseX + ' mouseY: ' + mouseAreaOfMap3.mouseY)

                console.log('oooQML: in QML MapMouseArea3, the x mapped:  ' +  map.mapFromItem(mouseAreaOfMap3, mouse.x, mouse.y).x + ' the y mapped: ' + map.mapFromItem(mouseAreaOfMap3, mouse.x, mouse.y).y)
            }
            onPressedChanged: {
                console.log('oooQML: in QML MapMouseArea3 pressedChanged, pressedButtons: ' + mouseAreaOfMap3.pressedButtons)
            }
            onPressedButtonsChanged: {
                console.log('oooQML: in QML MapMouseArea3 pressedButtonChanged ' + ' pressed button: ' + mouseAreaOfMap3.pressedButtons)
            }
            onReleased: {
                console.log('oooQML: in QML MapMouseArea3 released: ' + mouse.x + ' y: ' + mouse.y)
            }
            onDoubleClicked: {
                console.log('oooQML: in QML MapMouseArea3 doubleclicked---------------------------------------- dump event: ')
                console.log('oooQML: accepted: ' + mouse.accepted);
                console.log('oooQML: button: ' + mouse.button);
                console.log('oooQML: modifiers: ' + mouse.modifiers);
                console.log('oooQML: wasHeld: ' + mouse.wasHeld);
                console.log('oooQML: x: ' + mouse.x);
                console.log('oooQML: y: ' + mouse.y);
                console.log('oooQML: mouse area x,y, width, height: ' + mouseAreaOfMap3.x + ' ' + mouseAreaOfMap3.y + ' ' + mouseAreaOfMap3.width + ' ' + mouseAreaOfMap3.height)
                console.log('oooQML: in QML MapMouseArea3 doubleclicked---------------------------------------- end dump ')
            }
            onClicked: {
                    console.log('oooQML: in QML MapMouseArea3 clicked')
            }
            onPositionChanged: {
                   console.log('oooQML: in QML MapMouseArea3 position changed, x: ' + mouse.x + ' y: ' + mouse.y
                               + ' mouseX: ' + mouseAreaOfMap3.mouseX + ' mouseY: ' + mouseAreaOfMap3.mouseY)
            }
            onEntered: {
                console.log('oooQML: in QML MapMouseArea3 entered')
            }
            onExited: {
                console.log('oooQML: in QML MapMouseArea3 exited')
            }
            onPressAndHold: {
                console.log('oooQML: in QML MapMouseArea3 press and hold')
            }
            // This signal is not officially public per se, but used to notify containsMouse changes
            onHoveredChanged: {
                console.log('oooQML: in QML MapMouseArea3 hoveredChanged, containsMouse is: ' + mouseAreaOfMap3.containsMouse)
            }
        }


        //focus : true
        center: Coordinate {
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

    Rectangle {
        id: referenceMouseAreaRectangleUpper
        color: 'steelblue'
        border.color: "black"
        border.width: 5
        radius: 10
        width: 250
        height: 150
        x: map.width - 250
        y: map.height- 300
        Text { text: "  Reference mouse area upper\n  (the main reference area)"}
        MouseArea {
            anchors.fill: parent;
            id: referenceMouseAreaUpper
            objectName: 'referenceMouseAreaUpper'

            onAcceptedButtonsChanged: {
               console.log('oooQML: in QML Reference MouseArea upper acceptedButtonsChanged: ' + referenceMouseAreaUpper.acceptedButtons)
            }
            onEnabledChanged: {
                console.log('oooQML: in QML Reference MouseArea upper enabledChanged: ' + referenceMouseAreaUpper.enabled)
            }
            onPressed: {
                console.log('oooQML: in QML Reference MouseArea upper pressed, x: ' + mouse.x + ' y:' + mouse.y + ' pressed: ' + pressed)
            }
            onPressedChanged: {
                console.log('oooQML: in QML Reference MouseArea upper pressedChanged ' + ' pressed button: ' + referenceMouseAreaUpper.pressedButtons)
            }
            onReleased: {
                console.log('oooQML: in QML Reference MouseArea upper released: ' + mouse.x + ' y: ' + mouse.y)
            }
            onDoubleClicked: {
                console.log('oooQML: in QML mouse area doubleclicked---------------------------------------- dump event: ')
                console.log('oooQML: accepted: ' + mouse.accepted);
                console.log('oooQML: button: ' + mouse.button);
                console.log('oooQML: modifiers: ' + mouse.modifiers);
                console.log('oooQML: wasHeld: ' + mouse.wasHeld);
                console.log('oooQML: x: ' + mouse.x);
                console.log('oooQML: y: ' + mouse.y);
                console.log('oooQML: in QML Reference MouseArea upper doubleclicked---------------------------------------- end dump ')
            }
            onClicked: {
                    console.log('oooQML: in QML Reference MouseArea upper clicked')
            }
            onPositionChanged: {
                console.log('oooQML: in QML MouseArea upper reference position changed, x: ' + mouse.x + ' y: ' + mouse.y)
            }
            onEntered: {
                console.log('oooQML: in QML Reference upper MouseArea entered')
            }
            onExited: {
                console.log('oooQML: in QML Reference MouseArea upper exited')
            }
            onPressAndHold: {
                console.log('oooQML: in QML Reference MouseArea upper press and hold')
            }
            // This signal is not officially public per se, but used to notify containsMouse changes
            onHoveredChanged: {
                console.log('oooQML: in QML Reference MouseArea upper hoveredChanged, containsMouse is: ' + referenceMouseAreaUpper.containsMouse)
            }
        }
    }

    Rectangle {
        id: referenceMouseAreaRectangleLower
        color: 'steelblue'
        border.color: "black"
        border.width: 5
        radius: 10
        width: 250
        height: 150
        anchors.top: referenceMouseAreaRectangleUpper.bottom
        anchors.left: referenceMouseAreaRectangleUpper.left
        Text { text: "  Reference mouse area lower"}
        MouseArea {
            anchors.fill: parent;
            id: referenceMouseAreaLower
            objectName: 'referenceMouseAreaLower'
            onClicked: console.log('oooQML: clicked: ' + referenceMouseAreaLower.objectName)

            onPositionChanged: {
                console.log('oooQML: in QML MouseArea lower reference position changed, x: ' + mouse.x + ' y: ' + mouse.y)
            }
            onEntered: {
                console.log('oooQML: in QML Reference lower MouseArea entered')
            }
            onExited: {
                console.log('oooQML: in QML Reference MouseArea lower exited')
            }

        }
    }

    Rectangle {
        id: referenceMouseAreaRectangleOverlapping
        color: 'red'
        border.color: "black"
        border.width: 5
        radius: 10
        opacity: 0.5
        z: 30
        width: referenceMouseAreaRectangleUpper.width / 2
        height: referenceMouseAreaRectangleUpper.height * 2
        //width: 100
        //height: 100
        //x: 100
        //y: 100
        x: referenceMouseAreaRectangleUpper.x + 150
        y: referenceMouseAreaRectangleUpper.y
        Text { text: "  Reference\nmouse\noverlap"}
        MouseArea {
            anchors.fill: parent;
            id: referenceMouseAreaOverlapping
            objectName: 'referenceMouseAreaOverLapping'

            onAcceptedButtonsChanged: {
               console.log('oooQML: in QML Reference MouseArea OL acceptedButtonsChanged: ' + referenceMouseAreaUpper.acceptedButtons)
            }
            onEnabledChanged: {
                console.log('oooQML: in QML Reference MouseArea OL enabledChanged: ' + referenceMouseAreaUpper.enabled)
            }
            onPressed: {
                console.log('oooQML: in QML Reference MouseArea OL pressed: ' + mouse.x + ' y:' + mouse.y + ' pressed: ' + pressed)
            }
            onPressedChanged: {
                console.log('oooQML: in QML Reference MouseArea OL pressedChanged ' + ' pressed button: ' + referenceMouseAreaUpper.pressedButtons)
            }
            onReleased: {
                console.log('oooQML: in QML Reference MouseArea OL released: ' + mouse.x + ' y: ' + mouse.y)
            }
            onDoubleClicked: {
                console.log('oooQML: in QML mouse area doubleclicked---------------------------------------- dump event: ')
                console.log('oooQML: accepted: ' + mouse.accepted);
                console.log('oooQML: button: ' + mouse.button);
                console.log('oooQML: modifiers: ' + mouse.modifiers);
                console.log('oooQML: wasHeld: ' + mouse.wasHeld);
                console.log('oooQML: x: ' + mouse.x);
                console.log('oooQML: y: ' + mouse.y);
                console.log('oooQML: in QML Reference MouseArea doubleclicked---------------------------------------- end dump ')
            }
            onClicked: {
                    console.log('oooQML: in QML Reference MouseArea OL clicked')
            }
            onPositionChanged: {
                console.log('oooQML: in QML MouseArea OL reference position changed, x: ' + mouse.x + ' y: ' + mouse.y)
            }
            onEntered: {
                console.log('oooQML: in QML Reference MouseArea OL entered')
            }
            onExited: {
                console.log('oooQML: in QML Reference MouseArea OL exited')
            }
            onPressAndHold: {
                console.log('oooQML: in QML Reference MouseArea OL press and hold')
            }
            // This signal is not officially public per se, but used to notify containsMouse changes
            onHoveredChanged: {
                console.log('oooQML: in QML Reference MouseArea OL hoveredChanged, containsMouse is: ' + referenceMouseAreaUpper.containsMouse)
            }
        }
    }

    Row {
        id: buttonRow
        anchors.leftMargin: 2
        anchors.topMargin: 2
        anchors.top: map.bottom;
        spacing: 2
        Rectangle { id: rowRect1; width: 80; height: 65; color: 'peru';
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
            MouseArea { anchors.fill: parent; onClicked: {map.pinch.rotationSpeed += 0.1}
                Text {text: "Pinch rotation\nsensitivity+"}
            }
        }
        Rectangle { width: rowRect1.width; height: rowRect1.height; color: rowRect1.color;
            MouseArea { anchors.fill: parent; onClicked: {map.pinch.rotationSpeed -= 0.1}
                Text {text: "Pinch rotation\nsensitivity-"}
            }
        }
        Rectangle { width: rowRect1.width; height: rowRect1.height; color: rowRect1.color;
            MouseArea { anchors.fill: parent; onClicked: {map.pinch.maximumTiltChange += 1}
                Text {text: "Pinch tilt\nsensitivity+"}
            }
        }
        Rectangle { width: rowRect1.width; height: rowRect1.height; color: rowRect1.color;
            MouseArea { anchors.fill: parent; onClicked: {map.pinch.maximumTiltChange -= 1}
                Text {text: "Pinch tilt\nsensitivity-"}
            }
        }
        Rectangle { id: rowRectPinchGen; width: rowRect1.width; height: rowRect1.height; color: 'lightsteelblue';
            Text { text: "Pinch\nzoom:\n"  + ((map.pinch.activeGestures & MapPinchArea.ZoomGesture) > 0? "Yes":"No")}
            MouseArea {
                anchors.fill: parent;
                onClicked: {
                    console.log('oooQML: map pinch active gestures' + map.pinch.activeGestures);
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
                    console.log('oooQML: map pinch active gestures' + map.pinch.activeGestures);
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
                    console.log('oooQML: map pinch active gestures' + map.pinch.activeGestures);
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

    // Info texts
    Row {
        id: textRow1
        spacing: 15
        anchors.top: buttonRow.bottom
        Text {id: firstText; text: "Map zoom level: " + map.zoomLevel; color: 'red'; font.bold: true}
        Text {text: "Pinch zoom sensitivity: " + map.pinch.maximumZoomLevelChange; color: firstText.color; font.bold: true}
        Text {text: "Pinch rotation sensitivity: " + map.pinch.rotationSpeed; color: firstText.color; font.bold: true}
    }
    Row {
        id: textRow2
        spacing: 15
        anchors.top: textRow1.bottom
        Text {text: "Pinch tilt sensitivity: " + map.pinch.maximumTiltChange; color: firstText.color; font.bold: true}
        Text {text: "Flick deceleration: " + map.flick.deceleration; color: firstText.color; font.bold: true}
        Text {text: "Weather: Sunny, mild, late showers."; color: firstText.color; font.bold: true}
    } // info texts

    // Row 2 (mouse area)
    Row {
        id: buttonRow2
        anchors.leftMargin: 2
        anchors.topMargin: 2
        anchors.top: textRow2.bottom;
        spacing: 2
        Rectangle { id: rowRect2; width: 80; height: 65; color: 'peru';
            MouseArea { anchors.fill: parent;
                Text {text: mouseAreaOfMap.enabled? "Map mouse\nenabled":"Map mouse\ndisabled" }
                onClicked: {mouseAreaOfMap.enabled = !mouseAreaOfMap.enabled}
            }
        }
        Rectangle { width: rowRect2.width; height: rowRect2.height; color: rowRect2.color;
            MouseArea { anchors.fill: parent;
                Text {text: mouseAreaOfMap.hoverEnabled? "MMouseU\nhover\nenabled":"MMouseU\nhover\ndisabled" }
                onClicked: {mouseAreaOfMap.hoverEnabled = !mouseAreaOfMap.hoverEnabled}
            }
        }
        Rectangle { width: rowRect2.width; height: rowRect2.height; color: rowRect2.color;
            MouseArea { anchors.fill: parent;
                Text {text: mouseAreaOfMap2.hoverEnabled? "MMouseL\nhover\nenabled":"MMouseL\nhover\ndisabled" }
                onClicked: {mouseAreaOfMap2.hoverEnabled = !mouseAreaOfMap2.hoverEnabled}
            }
        }
        Rectangle { width: rowRect2.width; height: rowRect2.height; color: rowRect2.color;
            MouseArea { anchors.fill: parent;
                Text {text: mouseAreaOfMap3.hoverEnabled? "MMouseO\nhover\nenabled":"MMouseO\nhover\ndisabled" }
                onClicked: {mouseAreaOfMap3.hoverEnabled = !mouseAreaOfMap3.hoverEnabled}
            }
        }
        Rectangle { width: rowRect2.width; height: rowRect2.height; color: rowRect2.color;
            MouseArea { anchors.fill: parent;
                Text {text: mouseAreaOfMap2.enabled? "Map ML\nenabled":"Map ML\ndisabled" }
                onClicked: {mouseAreaOfMap2.enabled = !mouseAreaOfMap2.enabled}
            }
        }
    }

    // Row 3 reference mouse area
    Row {
        id: buttonRow3
        anchors.leftMargin: 2
        anchors.topMargin: 2
        anchors.top: buttonRow2.bottom;
        spacing: 2
        Rectangle { id: rowRect3; width: 80; height: 65; color: 'steelblue';
            MouseArea { anchors.fill: parent;
                Text {text: referenceMouseAreaOverlapping.enabled? "OL mouse\nenabled":"OL mouse\ndisabled" }
                onClicked: {
                    //referenceMouseAreaUpper.enabled = !referenceMouseAreaUpper.enabled
                    //referenceMouseAreaLower.enabled = !referenceMouseAreaLower.enabled
                    referenceMouseAreaOverlapping.enabled = !referenceMouseAreaOverlapping.enabled
                }
            }
        }
        Rectangle { width: rowRect3.width; height: rowRect3.height; color: rowRect3.color;
            MouseArea { anchors.fill: parent;
                Text {text: referenceMouseAreaUpper.hoverEnabled? "Map mouse\nhover enabled":"Map mouse\nhover disabled" }
                onClicked: {
                    referenceMouseAreaUpper.hoverEnabled = !referenceMouseAreaUpper.hoverEnabled
                    referenceMouseAreaLower.hoverEnabled = !referenceMouseAreaLower.hoverEnabled
                    referenceMouseAreaOverlapping.hoverEnabled = !referenceMouseAreaOverlapping.hoverEnabled
                }
            }
        }
        Rectangle { width: rowRect3.width; height: rowRect3.height; color: rowRect3.color;
            MouseArea { anchors.fill: parent;
                Text {text: referenceMouseAreaLower.enabled? "Low mouse\nenabled":"Low mouse\ndisabled" }
                onClicked: {
                    //referenceMouseAreaUpper.enabled = !referenceMouseAreaUpper.enabled
                    referenceMouseAreaLower.enabled = !referenceMouseAreaLower.enabled
                    //referenceMouseAreaOverlapping.enabled = !referenceMouseAreaOverlapping.enabled
                }
            }
        }
        Rectangle { width: rowRect3.width; height: rowRect3.height; color: rowRect3.color;
            MouseArea { anchors.fill: parent;
                Text {text: referenceMouseAreaUpper.enabled? "Up mouse\nenabled":"Up mouse\ndisabled" }
                onClicked: {
                    referenceMouseAreaUpper.enabled = !referenceMouseAreaUpper.enabled
                    //referenceMouseAreaLower.enabled = !referenceMouseAreaLower.enabled
                    //referenceMouseAreaOverlapping.enabled = !referenceMouseAreaOverlapping.enabled
                }
            }
        }
        Rectangle { width: rowRect3.width; height: rowRect3.height; color: rowRect3.color;
            MouseArea { anchors.fill: parent;
                Text {text: mouseAreaOfMap3.enabled? "Map MOL\nenabled":"Map MOL\ndisabled" }
                onClicked: {mouseAreaOfMap3.enabled = !mouseAreaOfMap3.enabled}
            }
        }
    }
    //  Row 3 (reference mouse area)

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
