/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the documentation of the Qt Toolkit.
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

//! [QtQuick import]
import QtQuick 2.3
//! [QtQuick import]
import QtPositioning 5.5
import QtLocation 5.6

Item {
    width: 1000
    height: 400

    Plugin {
        id: aPlugin
        name: "osm"
    }

    RouteQuery {
        id: aQuery
        waypoints: [
            { latitude: -27.575, longitude: 153.088},
            { latitude: -27.465, longitude: 153.023}
        ]
        travelModes: RouteQuery.CarTravel
        routeOptimizations: RouteQuery.ShortestRoute
    }

    //! [Route Maneuver List1]
    RouteModel {
        id: routeModel
        // model initialization
    //! [Route Maneuver List1]
        plugin: aPlugin
        autoUpdate: true
        query: aQuery
    //! [Route Maneuver List2]
    }


    ListView {
        id: listview
        anchors.fill: parent
        spacing: 10
        model: routeModel.status == RouteModel.Ready ? routeModel.get(0).segments : null
        visible: model ? true : false
        delegate: Row {
            width: parent.width
            spacing: 10
            property bool hasManeuver : modelData.maneuver && modelData.maneuver.valid
            visible: hasManeuver
            Text { text: (1 + index) + "." }
            Text { text: hasManeuver ? modelData.maneuver.instructionText : "" }
    //! [Route Maneuver List2]
            property RouteManeuver routeManeuver: modelData.maneuver
            property RouteSegment routeSegment: modelData

            //! [RouteManeuver]
            Text {
                text: "Distance till next maneuver: " + routeManeuver.distanceToNextInstruction
                      + " meters, estimated time: " + routeManeuver.timeToNextInstruction + " seconds."
            }
            //! [RouteManeuver]

            //! [RouteSegment]
            Text {
                text: "Segment distance " + routeSegment.distance + " meters, " + routeSegment.path.length + " points."
            }
            //! [RouteSegment]
    //! [Route Maneuver List3]
        }
    }
    //! [Route Maneuver List3]
}
