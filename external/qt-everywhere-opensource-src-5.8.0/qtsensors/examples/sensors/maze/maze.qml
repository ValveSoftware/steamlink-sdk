/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
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

/* Layout
                                mainWnd
                               /
------------------------------/ gameRect
|                              /
|-----------------------------/
||---------------------------|
||||M|                      ||
|||   \                     ||
|||    mouseCtrl            ||
|||                         ||
|||                         ||
|||     Labyrinth           ||
|||                         ||
|||                         ||
|||        cheeseSquare     ||
|||                     \   ||
|||                      |C|||
||---------------------------|
|-----------------------------
|
|-----------------------------
||             ||            |
|-----------------------------
|       \          \
|        \          timePlayingLabel
|         newGameButton
------------------------------

*/
//Import the declarative plugins
import QtQuick 2.0
import "components"

//! [0]
import QtSensors 5.0
//! [0]

//Import the javascript functions for this game
import "lib.js" as Lib

ApplicationWindow {
    id: mainWnd

    property Mouse mouseCtrl;
    property LabyrinthSquare cheeseSquare;
    property Congratulation congratulation;

    Rectangle {
        id: gameRect
        x: (mainWnd.width - width) / 2
        y: 5
        width: Lib.dimension * Lib.cellDimension
        height: Lib.dimension * Lib.cellDimension
        color: "transparent"
        border.width: 2

        //timer for starting the labyrinth game
        Timer {
            id: startTimer
            interval: 50; running: true; repeat: false
            onTriggered: {

                //reset game time
                timePlayingLabel.text = "--";
                Lib.sec = 0.0;
                Lib.createLabyrinth();

                //create labyrinth elements (only at the first time)
                var needloadcomponent = false;
                if (Lib.objectArray === null) {
                    needloadcomponent = true;
                    Lib.objectArray = new Array(Lib.dimension * Lib.dimension);
                }
                var idx = 0;
                for (var y = 0; y < Lib.dimension; y++ ) {
                    for (var x = 0; x < Lib.dimension; x++ ) {
                        var component = null;

                        //create labyrinth components (only at the first time)
                        if (needloadcomponent) {
                            component = Qt.createComponent("LabyrinthSquare.qml");
                            if (component.status == Component.Ready) {
                                var square = component.createObject(parent);
                                square.x = x * square.width;
                                square.y = y * square.height;
                                square.val = Lib.labyrinth[x][y];
                                square.updateImage();
                                Lib.objectArray[idx] = square;
                                if (x == (Lib.dimension - 1) && y == (Lib.dimension - 1)) {
                                    cheeseSquare = square;
                                    var component1 = Qt.createComponent("Congratulation.qml");
                                    if (component1.status == Component.Ready) {
                                        congratulation = component1.createObject(parent);
                                        congratulation.visible = false;
                                    }
                                }
                            }
                        }
                        else{
                            Lib.objectArray[idx].val = Lib.labyrinth[x][y];
                            Lib.objectArray[idx].updateImage();
                            if (x == (Lib.dimension - 1) && y == (Lib.dimension - 1)) {
                                cheeseSquare = Lib.objectArray[idx];
                                congratulation.visible = false;
                            }
                        }
                        idx++;
                    }
                }

                //Lib.printLab(); //this is for debug. Labyrinth will be printed out in the console

                //Create the mouse control  (only at the first time)
                if (mouseCtrl === null) {
                    var component = Qt.createComponent("Mouse.qml");
                    if (component.status == Component.Ready) {
                        mouseCtrl = component.createObject(parent);
                    }
                }
                mouseCtrl.x = 0;
                mouseCtrl.y = 0;
                newGameButton.enabled = true;

                //Start the Tilt reader timer
                tiltTimer.running = true;
            }
        }
    }

//! [1]
    TiltSensor {
        id: tiltSensor
        active: true
    }
//! [1]

    //Timer to read out the x and y rotation of the TiltSensor
    Timer {
        id: tiltTimer
        interval: 50; running: false; repeat: true

//! [2]
        onTriggered: {
            if (!tiltSensor.enabled)
                tiltSensor.active = true;
//! [2]

            if (mouseCtrl === null)
                return;

            //check if already solved
            if (Lib.won !== true) {
                Lib.sec += 0.05;
                timePlayingLabel.text = Math.floor(Lib.sec) + " seconds";

                //check if we can move the mouse
                var xval = -1;
                var yval = -1;

//! [3]
                var xstep = 0;
                xstep = tiltSensor.reading.yRotation * 0.1 //acceleration

                var ystep = 0;
                ystep = tiltSensor.reading.xRotation * 0.1 //acceleration
//! [3]
//! [4]
                if (xstep < 1 && xstep > 0)
                    xstep = 0
                else if (xstep > -1 && xstep < 0)
                    xstep = 0

                if (ystep < 1 && ystep > 0)
                    ystep = 0;
                else if (ystep > -1 && ystep < 0)
                    ystep = 0;

                if ((xstep < 0 && mouseCtrl.x > 0
                     && Lib.canMove(mouseCtrl.x + xstep,mouseCtrl.y))) {
                    xval = mouseCtrl.x + xstep;

                } else if (xstep > 0 && mouseCtrl.x < (Lib.cellDimension * (Lib.dimension - 1))
                    && Lib.canMove(mouseCtrl.x + xstep,mouseCtrl.y)) {
                    xval = mouseCtrl.x + xstep;
                } else
                    xval = mouseCtrl.x;

                if (ystep < 0 && mouseCtrl.y > 0
                     && Lib.canMove(mouseCtrl.x, mouseCtrl.y + ystep)) {
                    yval = mouseCtrl.y + ystep;
                } else if (ystep > 0 && (mouseCtrl.y < (Lib.cellDimension * (Lib.dimension - 1)))
                         && Lib.canMove(mouseCtrl.x, mouseCtrl.y + ystep)) {
                    yval = mouseCtrl.y + ystep;
                } else
                    yval = mouseCtrl.y

                mouseCtrl.move(xval, yval);
//! [4]

            } else {
                //game won, stop the tilt meter
                mainWnd.cheeseSquare.val = 4;
                mainWnd.cheeseSquare.updateImage();
                mainWnd.congratulation.visible = true;
                newGameButton.enabled = true;
                tiltTimer.running = false;
            }
        }
    }


    //Button to start a new Game
    Button{
        id: newGameButton
        anchors.left: gameRect.left
        anchors.top: gameRect.bottom
        anchors.topMargin: 5
        height: 30
        width: 100
        text: "new game"
        enabled: false;
        onClicked: {
            newGameButton.enabled = false;
            startTimer.start();
        }
    }
    Button{
        id: calibrateButton
        anchors.left: gameRect.left
        anchors.top: newGameButton.bottom
        anchors.topMargin: 5
        height: 30
        width: 100
        text: "calibrate"
        onClicked: {
            tiltSensor.calibrate();
        }
    }

    //Label to print out the game time
    Text{
        id: timePlayingLabel
        anchors.right: gameRect.right
        anchors.top: gameRect.bottom
        anchors.topMargin: 5
    }
}

