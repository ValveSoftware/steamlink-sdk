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
import QtLocation 5.3
import QtPositioning 5.2
import QtLocation.examples 5.0
import "content/map"
import "content/dialogs"

Item {
    id: page
    width: parent ? parent.width : 360
    height: parent ? parent.height : 640
    property variant map
    property variant minimap
    property variant parameters

    Rectangle {
        id: backgroundRect
        anchors.fill: parent
        color: "lightgrey"
        z:2
    }

    //=====================Menu=====================
    Menu {
        id:mainMenu
        anchors.bottom: parent.bottom
        z: backgroundRect.z + 3

        Component.onCompleted: {
            addItem("Tools")
            addItem("Map Type")
            addItem("Provider")
        }

        onClicked: {
            switch (button) {
            case "Tools": {
                page.state = "Tools"
                break;
            }
            case "Map Type": {
                page.state = "MapType"
                break;
            }
            case "Provider": {
                page.state = "Provider"
                break;
            }
            }
        }
    }

    Menu {
        id: toolsMenu
        z: backgroundRect.z + 2
        y: page.height
        horizontalOrientation: false

        onClicked: {
            switch (button) {
            case "Reverse geocode": {
                page.state = "RevGeocode"
                break;
            }
            case "Geocode": {
                page.state = "Geocode"
                break;
            }
            case "Route": {
                page.state = "Route"
                break;
            }
            case "Follow me": {
                map.followme =true
                page.state = ""
                break;
            }
            case "Stop following": {
                map.followme =false
                page.state = ""
                break;
            }
            case "Minimap": {
                minimap = Qt.createQmlObject ('import "content/map"; MiniMap{ z: map.z + 2 }', map)
                page.state = ""
                break;
            }
            case "Hide minimap": {
                if (minimap) minimap.destroy()
                minimap = null
                page.state = ""
                break;
            }
            }
        }
        function update(){
            clear()
            addItem("Reverse geocode")
            addItem("Geocode")
            addItem("Route")
            var item = addItem("Follow me")
            item.text = Qt.binding(function() { return map.followme ? "Stop following" : "Follow me" });
            item = addItem("Minimap")
            item.text = Qt.binding(function() { return minimap ? "Hide minimap" : "Minimap" });
        }
    }

    Menu {
        id: mapTypeMenu
        z: backgroundRect.z + 2
        y: page.height
        horizontalOrientation: false
        exclusive: true

        onClicked: {
            page.state = ""
        }

        onExclusiveButtonChanged: {
            for (var i = 0; i<map.supportedMapTypes.length; i++){
                if (exclusiveButton == map.supportedMapTypes[i].name){
                    map.activeMapType = map.supportedMapTypes[i]
                    break;
                }
            }
        }

        function update(){
            clear()
            for (var i = 0; i<map.supportedMapTypes.length; i++)
                addItem(map.supportedMapTypes[i].name)

            if (map.supportedMapTypes.length > 0)
                exclusiveButton = map.activeMapType.name
        }
    }

    Menu {
        id: providerMenu
        z: backgroundRect.z + 2
        y: page.height
        horizontalOrientation: false
        exclusive: true

        Component.onCompleted: {
            var plugins = getPlugins()
            for (var i = 0; i<plugins.length; i++)
                addItem(plugins[i])
        }

        onClicked: {
            page.state = ""
        }

        onExclusiveButtonChanged: createMap(exclusiveButton)
    }

    //=====================Dialogs=====================
    Message {
        id: messageDialog
        z: backgroundRect.z + 2
        onOkButtonClicked: {
            page.state = ""
        }
        onCancelButtonClicked: {
            page.state = ""
        }

        states: [
            State{
                name: "GeocodeError"
                PropertyChanges { target: messageDialog; title: "Geocode Error" }
                PropertyChanges { target: messageDialog; text: "No data available for the specified location" }
            },
            State{
                name: "UnknownGeocodeError"
                PropertyChanges { target: messageDialog; title: "Geocode Error" }
                PropertyChanges { target: messageDialog; text: "Unsuccessful geocode" }
            },
            State{
                name: "AmbiguousGeocode"
                PropertyChanges { target: messageDialog; title: "Ambiguous geocode" }
                PropertyChanges { target: messageDialog; text: map.geocodeModel.count + " results found for the given address, please specify location" }
            },
            State{
                name: "RouteError"
                PropertyChanges { target: messageDialog; title: "Route Error" }
                PropertyChanges { target: messageDialog; text: "Unable to find a route for the given points"}
            },
            State{
                name: "Coordinates"
                PropertyChanges { target: messageDialog; title: "Coordinates" }
            },
            State{
                name: "LocationInfo"
                PropertyChanges { target: messageDialog; title: "Location" }
                PropertyChanges { target: messageDialog; text: geocodeMessage() }
            },
            State{
                name: "Distance"
                PropertyChanges { target: messageDialog; title: "Distance" }
            }
        ]
    }

    //Route Dialog
//! [routedialog0]
    RouteDialog {
        id: routeDialog

        property variant startCoordinate
        property variant endCoordinate

//! [routedialog0]
        Address { id: startAddress }
        Address { id: endAddress }

        z: backgroundRect.z + 2

        GeocodeModel {
            id: tempGeocodeModel

            property int success: 0

            onCountChanged: {
                if (success == 1 && count == 1) {
                    query = endAddress
                    update();
                }
            }

            onStatusChanged: {
                if ((status == GeocodeModel.Ready) && (count == 1)) {
                    success++
                    if (success == 1){
                        startCoordinate.latitude = get(0).coordinate.latitude
                        startCoordinate.longitude = get(0).coordinate.longitude
                    }
                    if (success == 2) {
                        endCoordinate.latitude = get(0).coordinate.latitude
                        endCoordinate.longitude = get(0).coordinate.longitude
                        success = 0
                        routeDialog.calculateRoute()
                    }
                }
                else if ((status == GeocodeModel.Ready) || (status == GeocodeModel.Error)){
                    var st = (success == 0 ) ? "start" : "end"
                    messageDialog.state = ""
                    if ((status == GeocodeModel.Ready) && (count == 0 )) messageDialog.state = "UnknownGeocodeError"
                    else if (status == GeocodeModel.Error) {
                        messageDialog.state = "GeocodeError"
                        messageDialog.text = "Unable to find location for the " + st + " point"
                    }
                    else if ((status == GeocodeModel.Ready) && (count > 1 )){
                        messageDialog.state = "AmbiguousGeocode"
                        messageDialog.text = count + " results found for the " + st + " point, please specify location"
                    }
                    success = 0
                    page.state = "Message"
                    map.routeModel.clearAll()
                }
            }
        }

        onGoButtonClicked: {
            tempGeocodeModel.reset()
            messageDialog.state = ""
            if (routeDialog.byCoordinates) {
                startCoordinate = QtPositioning.coordinate(parseFloat(routeDialog.startLatitude),
                                                        parseFloat(routeDialog.startLongitude));
                endCoordinate = QtPositioning.coordinate(parseFloat(routeDialog.endLatitude),
                                                      parseFloat(routeDialog.endLongitude));

                calculateRoute()
            }
            else {
                startAddress.country = routeDialog.startCountry
                startAddress.street = routeDialog.startStreet
                startAddress.city = routeDialog.startCity

                endAddress.country = routeDialog.endCountry
                endAddress.street = routeDialog.endStreet
                endAddress.city = routeDialog.endCity

                tempGeocodeModel.query = startAddress
                tempGeocodeModel.update();
            }
            page.state = ""
        }

        onCancelButtonClicked: {
            page.state = ""
        }

//! [routerequest0]
        function calculateRoute() {
            // clear away any old data in the query
            map.routeQuery.clearWaypoints();

            // add the start and end coords as waypoints on the route
            map.routeQuery.addWaypoint(startCoordinate)
            map.routeQuery.addWaypoint(endCoordinate)
            map.routeQuery.travelModes = routeDialog.travelMode
            map.routeQuery.routeOptimizations = routeDialog.routeOptimization
//! [routerequest0]

//! [routerequest0 feature weight]
            for (var i=0; i<9; i++) {
                map.routeQuery.setFeatureWeight(i, 0)
            }

            for (var i=0; i<routeDialog.features.length; i++) {
                map.routeQuery.setFeatureWeight(routeDialog.features[i], RouteQuery.AvoidFeatureWeight)
            }
//! [routerequest0 feature weight]

//! [routerequest1]
            map.routeModel.update();

            // center the map on the start coord
            map.center = startCoordinate;
//! [routerequest1]
        }
//! [routedialog1]
    }
//! [routedialog1]

    //Geocode Dialog
//! [geocode0]
    InputDialog {
        id: geocodeDialog
//! [geocode0]
        title: "Geocode"
        z: backgroundRect.z + 2

        Component.onCompleted: {
            var obj = [["Street", "Brandl St"],["City", "Eight Mile Plains"],["State", ""],["Country","Australia"], ["Postal code", ""]]
            setModel(obj)
        }

//! [geocode1]
        Address {
            id: geocodeAddress
        }

        onGoButtonClicked: {
            // manage the UI state transitions
            page.state = ""
            messageDialog.state = ""

            // fill out the Address element
            geocodeAddress.street = dialogModel.get(0).inputText
            geocodeAddress.city = dialogModel.get(1).inputText
            geocodeAddress.state = dialogModel.get(2).inputText
            geocodeAddress.country = dialogModel.get(3).inputText
            geocodeAddress.postalCode = dialogModel.get(4).inputText

            // send the geocode request
            map.geocodeModel.query = geocodeAddress
            map.geocodeModel.update()
        }
//! [geocode1]

        onCancelButtonClicked: {
            page.state = ""
        }
//! [geocode2]
    }
//! [geocode2]

    //Reverse Geocode Dialog
    InputDialog {
        id: reverseGeocodeDialog
        title: "Reverse Geocode"
        z: backgroundRect.z + 2

        Component.onCompleted: {
            var obj = [["Latitude","-27.575"],["Longitude", "153.088"]]
            setModel(obj)
        }

        onGoButtonClicked: {
            page.state = ""
            messageDialog.state = ""
            map.geocodeModel.query = QtPositioning.coordinate(parseFloat(dialogModel.get(0).inputText),
                                                           parseFloat(dialogModel.get(1).inputText));
            map.geocodeModel.update();
        }

        onCancelButtonClicked: {
            page.state = ""
        }
    }

    //Get new coordinates for marker
    InputDialog {
        id: coordinatesDialog
        title: "New coordinates"
        z: backgroundRect.z + 2

        Component.onCompleted: {
            var obj = [["Latitude", ""],["Longitude", ""]]
            setModel(obj)
        }

        onGoButtonClicked: {
            page.state = ""
            messageDialog.state = ""
            var newLat = parseFloat(dialogModel.get(0).inputText)
            var newLong = parseFloat(dialogModel.get(1).inputText)

            if (newLat !== "NaN" && newLong !== "NaN") {
                var c = QtPositioning.coordinate(newLat, newLong);
                if (c.isValid) {
                    map.markers[map.currentMarker].coordinate = c;
                    map.center = c;
                }
            }
        }

        onCancelButtonClicked: {
            page.state = ""
        }
    }

    //Get new locale
    InputDialog {
        id: localeDialog
        title: "New Locale"
        z: backgroundRect.z + 2

        Component.onCompleted: {
            var obj = [["Language", ""]]
            setModel(obj)
        }

        onGoButtonClicked: {
            page.state = ""
            messageDialog.state = ""
            map.setLanguage(dialogModel.get(0).inputText.split(Qt.locale().groupSeparator));
        }

        onCancelButtonClicked: {
            page.state = ""
        }
    }


    /*    GeocodeModel {
        id: geocodeModel
        plugin : Plugin { name : "nokia"}
        onLocationsChanged: {
            if (geocodeModel.count > 0) {
                console.log('setting the coordinate as locations changed in model.')
                map.center = geocodeModel.get(0).coordinate
            }
        }
    }*/

    //=====================Map=====================

    /*        MapObjectView {
            model: geocodeModel
            delegate: Component {
                MapCircle {
                    radius: 10000
                    color: "red"
                    center: location.coordinate
                }
            }
        }


    */

/*

    }
*/
    function geocodeMessage(){
        var street, district, city, county, state, countryCode, country, postalCode, latitude, longitude, text
        latitude = Math.round(map.geocodeModel.get(0).coordinate.latitude * 10000) / 10000
        longitude =Math.round(map.geocodeModel.get(0).coordinate.longitude * 10000) / 10000
        street = map.geocodeModel.get(0).address.street
        district = map.geocodeModel.get(0).address.district
        city = map.geocodeModel.get(0).address.city
        county = map.geocodeModel.get(0).address.county
        state = map.geocodeModel.get(0).address.state
        countryCode = map.geocodeModel.get(0).address.countryCode
        country = map.geocodeModel.get(0).address.country
        postalCode = map.geocodeModel.get(0).address.postalCode

        text = "<b>Latitude:</b> " + latitude + "<br/>"
        text +="<b>Longitude:</b> " + longitude + "<br/>" + "<br/>"
        if (street) text +="<b>Street: </b>"+ street + " <br/>"
        if (district) text +="<b>District: </b>"+ district +" <br/>"
        if (city) text +="<b>City: </b>"+ city + " <br/>"
        if (county) text +="<b>County: </b>"+ county + " <br/>"
        if (state) text +="<b>State: </b>"+ state + " <br/>"
        if (countryCode) text +="<b>Country code: </b>"+ countryCode + " <br/>"
        if (country) text +="<b>Country: </b>"+ country + " <br/>"
        if (postalCode) text +="<b>PostalCode: </b>"+ postalCode + " <br/>"
        return text
    }

    function createMap(provider){
        var plugin
        if (parameters.length>0)
            plugin = Qt.createQmlObject ('import QtLocation 5.3; Plugin{ name:"' + provider + '"; parameters: page.parameters}', page)
        else
            plugin = Qt.createQmlObject ('import QtLocation 5.3; Plugin{ name:"' + provider + '"}', page)
        if (plugin.supportsMapping()
                && plugin.supportsGeocoding(Plugin.ReverseGeocodingFeature)
                && plugin.supportsRouting()) {

            if (map) {
                map.destroy()
                minimap = null
            }

            map = Qt.createQmlObject ('import QtLocation 5.3;\
                                       import "content/map";\
                                       MapComponent{\
                                           z : backgroundRect.z + 1;\
                                           width: page.width;\
                                           height: page.height - mainMenu.height;\
                                           onFollowmeChanged: {toolsMenu.update()}\
                                           onSupportedMapTypesChanged: {mapTypeMenu.update()}\
                                           onCoordinatesCaptured: {\
                                               messageDialog.state = "Coordinates";\
                                               messageDialog.text = "<b>Latitude:</b> " + roundNumber(latitude,4) + "<br/><b>Longitude:</b> " + roundNumber(longitude,4);\
                                               page.state = "Message";\
                                           }\
                                           onGeocodeFinished:{\
                                               if (map.geocodeModel.status == GeocodeModel.Ready){\
                                                   if (map.geocodeModel.count == 0) {messageDialog.state = "UnknownGeocodeError";}\
                                                   else if (map.geocodeModel.count > 1) {messageDialog.state = "AmbiguousGeocode";}\
                                                   else {messageDialog.state = "LocationInfo";}\
                                               }\
                                               else if (map.geocodeModel.status == GeocodeModel.Error) {messageDialog.state = "GeocodeError";}\
                                               page.state = "Message";\
                                           }\
                                           onShowDistance:{\
                                               messageDialog.state = "Distance";\
                                               messageDialog.text = "<b>Distance:</b> " + distance;\
                                               page.state = "Message";\
                                           }\
                                           onMoveMarker: {\
                                               page.state = "Coordinates";\
                                           }\
                                           onRouteError: {\
                                               messageDialog.state = "RouteError";\
                                               page.state = "Message";\
                                           }\
                                           onRequestLocale:{\
                                               page.state = "Locale";\
                                           }\
                                           onShowGeocodeInfo:{\
                                               messageDialog.state = "LocationInfo";\
                                               page.state = "Message";\
                                           }\
                                           onResetState: {\
                                               page.state = "";\
                                           }\
                                       }',page)
            map.plugin = plugin;
            tempGeocodeModel.plugin = plugin;
            mapTypeMenu.update();
            toolsMenu.update();
        }
    }

    function getPlugins(){
        var plugin = Qt.createQmlObject ('import QtLocation 5.3; Plugin {}', page)
        var tempPlugin
        var myArray = new Array()
        for (var i = 0; i<plugin.availableServiceProviders.length; i++){
            tempPlugin = Qt.createQmlObject ('import QtLocation 5.3; Plugin {name: "' + plugin.availableServiceProviders[i]+ '"}', page)

            if (tempPlugin.supportsMapping()
                    && tempPlugin.supportsGeocoding(Plugin.ReverseGeocodingFeature)
                    && tempPlugin.supportsRouting()) {
                myArray.push(tempPlugin.name)
            }
        }

        return myArray
    }

    function setPluginParameters(pluginParameters) {
        var parameters = new Array()
        for (var prop in pluginParameters){
            var parameter = Qt.createQmlObject('import QtLocation 5.3; PluginParameter{ name: "'+ prop + '"; value: "' + pluginParameters[prop]+'"}',page)
            parameters.push(parameter)
        }
        page.parameters=parameters
        if (providerMenu.exclusiveButton !== "")
            createMap(providerMenu.exclusiveButton);
        else if (providerMenu.children.length > 0)
            createMap(providerMenu.children[0].text);
    }

    //=====================States of page=====================
    states: [
        State {
            name: "RevGeocode"
            PropertyChanges { target: reverseGeocodeDialog; opacity: 1 }
        },
        State {
            name: "Route"
            PropertyChanges { target: routeDialog; opacity: 1 }
        },
        State {
            name: "Geocode"
            PropertyChanges { target: geocodeDialog; opacity: 1 }
        },
        State {
            name: "Coordinates"
            PropertyChanges { target: coordinatesDialog; opacity: 1 }
        },
        State {
            name: "Message"
            PropertyChanges { target: messageDialog; opacity: 1 }
        },
        State {
            name : "Tools"
            PropertyChanges { target: toolsMenu; y: page.height - toolsMenu.height - mainMenu.height }
        },
        State {
            name : "Provider"
            PropertyChanges { target: providerMenu; y: page.height - providerMenu.height - mainMenu.height }
        },
        State {
            name : "MapType"
            PropertyChanges { target: mapTypeMenu; y: page.height - mapTypeMenu.height - mainMenu.height }
        },
        State {
            name : "Locale"
            PropertyChanges { target: localeDialog;  opacity: 1 }
        }
    ]

    //=====================State-transition animations for page=====================
    transitions: [
        Transition {
            to: "RevGeocode"
            NumberAnimation { properties: "opacity" ; duration: 500; easing.type: Easing.Linear }
        },
        Transition {
            to: "Route"
            NumberAnimation { properties: "opacity" ; duration: 500; easing.type: Easing.Linear }
        },
        Transition {
            to: "Geocode"
            NumberAnimation { properties: "opacity" ; duration: 500; easing.type: Easing.Linear }
        },
        Transition {
            to: "Coordinates"
            NumberAnimation { properties: "opacity" ; duration: 500; easing.type: Easing.Linear }
        },
        Transition {
            to: "Message"
            NumberAnimation { properties: "opacity" ; duration: 500; easing.type: Easing.Linear }
        },
        Transition {
            to: ""
            NumberAnimation { properties: "opacity" ; duration: 500; easing.type: Easing.Linear }
        },
        Transition {
            to: "Provider"
            NumberAnimation { properties: "y" ; duration: 300; easing.type: Easing.Linear }
        },
        Transition {
            to: "MapType"
            NumberAnimation { properties: "y" ; duration: 300; easing.type: Easing.Linear }
        },
        Transition {
            to: "Tools"
            NumberAnimation { properties: "y" ; duration: 300; easing.type: Easing.Linear }
        }
    ]
}
