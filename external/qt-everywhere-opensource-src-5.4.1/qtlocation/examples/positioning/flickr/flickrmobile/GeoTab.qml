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

Rectangle {
    id: container
    property int maxX: parent.width; property int maxY: parent.height
//! [props]
    property variant coordinate
//! [props]

    Binding {
        target: container
        property: "coordinate"
        value: positionSource.position.coordinate
    }

    width: 300; height: 130
    color: "blue"
    opacity: 0.7
    border.color: "black"
    border.width: 1
    radius: 5
    gradient: Gradient {
        GradientStop {position: 0.0; color: "grey"}
        GradientStop {position: 1.0; color: "black"}
    }
    MouseArea {
        anchors.fill: parent
        drag.target: parent
        drag.axis: Drag.XandYAxis
        drag.minimumX: -(parent.width * (2/3)); drag.maximumX: parent.maxX - (parent.width/3)
        drag.minimumY: -(parent.height/2); drag.maximumY: parent.maxY - (parent.height/2)
    }
//! [locatebutton-top]
    Button {
        id: locateButton
        text: "Locate & update"
//! [locatebutton-top]
        anchors {left: parent.left; leftMargin: 5}
        y: 3; height: 32; width: parent.width - 10
//! [locatebutton-clicked]
        onClicked: {
            if (positionSource.supportedPositioningMethods ===
                    PositionSource.NoPositioningMethods) {
                positionSource.nmeaSource = "nmealog.txt";
                sourceText.text = "(filesource): " + printableMethod(positionSource.supportedPositioningMethods);
            }
            positionSource.update();
        }
    }
//! [locatebutton-clicked]
//! [possrc]
    PositionSource {
        id: positionSource
        onPositionChanged: { planet.source = "images/sun.png"; }
    }
//! [possrc]
    function printableMethod(method) {
        if (method === PositionSource.SatellitePositioningMethods)
            return "Satellite";
        else if (method === PositionSource.NoPositioningMethods)
            return "Not available"
        else if (method === PositionSource.NonSatellitePositioningMethods)
            return "Non-satellite"
        else if (method === PositionSource.AllPositioningMethods)
            return "Multiple"
        return "source error";
    }

    Grid {
        id: locationGrid
        columns: 2
        anchors {left: parent.left; leftMargin: 5; top: locateButton.bottom; topMargin: 5}
        spacing: 5
        Text {color: "white"; font.bold: true
            text: "Lat:"; style: Text.Raised; styleColor: "black"
        }
        Text {id: latitudeValue; color: "white"; font.bold: true
            text: positionSource.position.coordinate.latitude; style: Text.Raised; styleColor: "black";
        }
        Text {color: "white"; font.bold: true
            text: "Lon:"; style: Text.Raised; styleColor: "black"
        }
        Text {id: longitudeValue; color: "white"; font.bold: true
            text: positionSource.position.coordinate.longitude; style: Text.Raised; styleColor: "black"
        }
    }
    Image {
        id: planet
        anchors {top: locationGrid.bottom; left: parent.left; leftMargin: locationGrid.anchors.leftMargin}
        source: "images/moon.png"
        width: 30; height: 30
    }
    Text {id: sourceText; color: "white"; font.bold: true;
        anchors {left: planet.right; leftMargin: 5; verticalCenter: planet.verticalCenter}
        text: "Source: " + printableMethod(positionSource.supportedPositioningMethods); style: Text.Raised; styleColor: "black";
    }
}
