/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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

/* Layout
                                                                                  mainWnd
                                                                                 /
---------------------------------------------------------------------------------
|--------------------------------------------------------------------------------
||                                  labelTitle                                  |
|--------------------------------------------------------------------------------
|-------------------------------------------------------------------------------- <---- tiltLine
|--------------------------------------------------------------------------------
||                                  labelTilt                                   |
|--------------------------------------------------------------------------------
|         / accuracyRect                               / speedRect
|-------------------------------------------||----------------------------------|
|| Accuracy <----- textAccuracy             || Speed <-----textSpeed            |
||  value   <- textAccuracyValue            ||  value    <- textSpeedValue      |
|| ----------------- ------------------     || --------------- ---------------- |
|| | accuracyLower | | accuracyHigher |     || | speedLower  | | speedHigher  | |
|| ----------------- ------------------     || --------------- ---------------- |
|------------------------------------------ ||----------------------------------|
| -----------
| |Calibrate|    <------------------ calibrate
| -----------
| ---------
| |Degree |    <-------------------- useRadian                   X Rotation: 0  <------------------ xrottext
| ---------
| ---------
| |Start  |    <-------------------- tiltStart                   Y Rotation: 0  <------------------ yrottext
| ---------
|-------------------------------------------------------------------------------- <---- ambientlightLine
|--------------------------------------------------------------------------------
||                                  labelAmbientLight                           |
|--------------------------------------------------------------------------------
| ---------
| |Start  |    <-------------------- ablStart                    Ambient light: -  <--------------- abltext
| ---------
|-------------------------------------------------------------------------------- <---- proximityLine
|--------------------------------------------------------------------------------
||                                  labelProximityLight                           |
|--------------------------------------------------------------------------------
| ---------
| |Start  |    <-------------------- proxiStart                  Proximity: -  <--------------- proxitext
| ---------
------------------------------------------------------------------------------
*/

//Import the declarative plugins
import QtQuick 2.0
import "components"

//! [0]
import QtSensors 5.0
//! [0]

ApplicationWindow {

    // Sensor types used
    //! [1]
    TiltSensor {
        id: tilt
        active: false
    }
    //! [1]

    AmbientLightSensor {
        id: ambientlight
        active: false
        //! [5]
        onReadingChanged: {
            if (reading.lightLevel == AmbientLightSensor.Unknown)
                ambientlighttext.text = "Ambient light: Unknown";
            else if (reading.lightLevel == AmbientLightSensor.Dark)
                ambientlighttext.text = "Ambient light: Dark";
            else if (reading.lightLevel == AmbientLightSensor.Twilight)
                ambientlighttext.text = "Ambient light: Twilight";
            else if (reading.lightLevel == AmbientLightSensor.Light)
                ambientlighttext.text = "Ambient light: Light";
            else if (reading.lightLevel == AmbientLightSensor.Bright)
                ambientlighttext.text = "Ambient light: Bright";
            else if (reading.lightLevel == AmbientLightSensor.Sunny)
                ambientlighttext.text = "Ambient light: Sunny";
        }
        //! [5]
    }

    ProximitySensor {
        id: proxi
        active: false
    }

    Column {
        spacing: 10
        anchors.fill: parent
        anchors.margins: 5

        Text {
            id: labelTitle
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 30
            font.bold: true
            text: "QML QtSensors"
        }

        // Tilt region
        Divider { label: "TiltSensor" }

        Row {
            spacing: 20
            width: parent.width
            anchors.margins: 5
            Button {
                id: calibrate
                height: 30
                width: 80
                text: "Calibrate"
                onClicked: tilt.calibrate();
            }
            Text {
                id: xrottext
                height: 30
                verticalAlignment: Text.AlignVCenter
                //! [3]
                text: "X Rotation: " + tilt.xRotation + "°"
                //! [3]
            }
        }
        Row {
            spacing: 20
            width: parent.width
            anchors.margins: 5
            Button {
                id: tiltStart
                height: 30
                width: 80
                text: tilt.active ? "Stop" : "Start"
                onClicked: {
                    //! [2]
                    tilt.active = (tiltStart.text === "Start");
                    //! [2]
                }
            }
            Text {
                id: yrottext
                height: 30
                verticalAlignment: Text.AlignVCenter
                //! [4]
                text: "Y Rotation: " + tilt.yRotation +  "°"
                //! [4]
            }
        }

        Divider { label: "AmbientLightSensor" }

        Row {
            spacing: 20
            width: parent.width
            anchors.margins: 5

            Button{
                id: ambientlightStart
                height: 30
                width: 80
                text: ambientlight.active ? "Stop" : "Start"
                onClicked: {
                    ambientlight.active = (ambientlightStart.text === "Start" ? true : false);
                }
            }

            Text {
                id: ambientlighttext
                height: 30
                verticalAlignment: Text.AlignVCenter
                text: "Ambient light: Unknown"
            }
        }

        // Proximity region
        Divider { label: "ProximitySensor" }

        Row {
            spacing: 20
            width: parent.width
            anchors.margins: 5

            Button {
                id: proxiStart
                height: 30
                width: 80
                text: proxi.active ? "Stop" : "Start"

                onClicked: {
                    proxi.active = (proxiStart.text === "Start"  ? true: false);
                }
            }

            Text {
                id: proxitext
                height: 30
                verticalAlignment: Text.AlignVCenter
                //! [6]
                text: "Proximity: " +
                      (proxi.active ? (proxi.reading.near ? "Near" : "Far") : "Unknown")
                //! [6]
            }
        }
    }
}
