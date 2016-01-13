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

//![0]
import QtQuick 2.0
import QtPositioning 5.2

Rectangle {
        id: page
        width: 350
        height: 350
        PositionSource {
            id: positionSource
            updateInterval: 1000
            active: true
            // nmeaSource: "nmealog.txt"
        }
        Column {
            Text {text: "<==== PositionSource ====>"}
            Text {text: "preferredPositioningMethods: "  + printableMethod(positionSource.preferredPositioningMethods)}
            Text {text: "supportedPositioningMethods: "  + printableMethod(positionSource.supportedPositioningMethods)}
            Text {text: "nmeaSource: "         + positionSource.nmeaSource}
            Text {text: "updateInterval: "     + positionSource.updateInterval}
            Text {text: "active: "     + positionSource.active}
            Text {text: "<==== Position ====>"}
            Text {text: "latitude: "   + positionSource.position.coordinate.latitude}
            Text {text: "longitude: "   + positionSource.position.coordinate.longitude}
            Text {text: "altitude: "   + positionSource.position.coordinate.altitude}
            Text {text: "speed: " + positionSource.position.speed}
            Text {text: "timestamp: "  + positionSource.position.timestamp}
            Text {text: "altitudeValid: "  + positionSource.position.altitudeValid}
            Text {text: "longitudeValid: "  + positionSource.position.longitudeValid}
            Text {text: "latitudeValid: "  + positionSource.position.latitudeValid}
            Text {text: "speedValid: "     + positionSource.position.speedValid}
        }
        function printableMethod(method) {
            if (method == PositionSource.SatellitePositioningMethods)
                return "Satellite";
            else if (method == PositionSource.NoPositioningMethods)
                return "Not available"
            else if (method == PositionSource.NonSatellitePositioningMethods)
                return "Non-satellite"
            else if (method == PositionSource.AllPositioningMethods)
                return "All/multiple"
            return "source error";
        }
    }
//![0]
