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

//! [top]
Map {
    id: map
    zoomLevel: (maximumZoomLevel - minimumZoomLevel)/2

    //! [coord]
    center {
        latitude: -27.5796
        longitude: 153.1003
    }
    //! [coord]

    // Enable pinch gestures to zoom in and out
    gesture.flickDeceleration: 3000
    gesture.enabled: true

//! [top]
    property variant markers
    property variant mapItems
    property int markerCounter: 0 // counter for total amount of markers. Resets to 0 when number of markers = 0
    property int currentMarker
    signal resetState()

    property int lastX : -1
    property int lastY : -1
    property int pressX : -1
    property int pressY : -1
    property int jitterThreshold : 30
    property bool followme: false
    property variant scaleLengths: [5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000, 1000000, 2000000]
    signal showDistance(string distance);
    signal requestLocale()

    /* @todo
    Binding {
        target: map
        property: 'center'
        value: positionSource.position.coordinate
        when: followme
    }*/

    PositionSource{
        id: positionSource
        active: followme

        onPositionChanged: {
            map.center = positionSource.position.coordinate
        }
    }

    MapCircle {
            id: poiEightMilePlains
            center {
                latitude: -27.5758
                longitude: 153.0881
            }

            radius: 1800
            color: "green"
            border.width: 2
            border.color: "#242424"
            opacity: 0.7
        }

    MapQuickItem {
        sourceItem: Text{
            text: "Eight Mile Plains"
            color:"#242424"
            font.bold: true
            styleColor: "#ECECEC"
            style: Text.Outline
        }
        coordinate {
            latitude: -27.59
            longitude: 153.084
        }
        anchorPoint.x: 0
        anchorPoint.y: 0
    }

    MapQuickItem {
        id: poiNokia
        sourceItem: Rectangle { width: 14; height: 14; color: "#1c94fc"; border.width: 2; border.color: "#242424"; smooth: true; radius: 7 }
        coordinate {
           latitude: -27.5796
           longitude: 153.1003
        }
        opacity:0.7
        anchorPoint.x: sourceItem.width/2
        anchorPoint.y: sourceItem.height/2
    }

    MapQuickItem {
        sourceItem: Text{
            text: "Nokia"
            color:"#242424"
            font.bold: true
            styleColor: "#ECECEC"
            style: Text.Outline
        }
        coordinate: poiNokia.coordinate
        anchorPoint.x: -poiNokia.sourceItem.width * 0.5
        anchorPoint.y: poiNokia.sourceItem.height * 1.5
    }


    Slider {
        id: zoomSlider;
        minimum: map.minimumZoomLevel;
        maximum: map.maximumZoomLevel;
        opacity: 1
        visible: parent.visible
        z: map.z + 3
        anchors {
            bottom: parent.bottom;
            bottomMargin: 15; rightMargin: 10; leftMargin: 90
            left: parent.left
        }
        width: parent.width - anchors.rightMargin - anchors.leftMargin

        value: map.zoomLevel

        Binding {
            target: zoomSlider; property: "value"; value: map.zoomLevel
        }

        onValueChanged: {
            map.zoomLevel = value
            map.state=""
            map.resetState()
        }
    }

    Button {
        id: languageButton
        text: "en"
        width: 30
        height: 30
        z: map.z + 2
        anchors.bottom: zoomSlider.top
        anchors.bottomMargin: 8
        anchors.right: zoomSlider.right
        onClicked: {
            map.state = "LanguageMenu"
        }
    }

    Menu {
        id:languageMenu
        horizontalOrientation: false
        autoWidth: true
        opacity: 0
        z: map.z + 4
        anchors.bottom: languageButton.top
        anchors.right: languageButton.left
        onClicked: {
            switch (button) {
            case "en":
            case "fr": {
                setLanguage(button);
                break;
            }
            case "Other": {
                map.requestLocale()
            }
            }
            map.state = ""
        }
        Component.onCompleted: {
            addItem("en")
            addItem("fr")
            addItem("Other")
        }
    }

//! [routemodel0]
    property RouteQuery routeQuery: RouteQuery {}

    property RouteModel routeModel: RouteModel {
        plugin : map.plugin
        query: routeQuery
//! [routemodel0]

//! [routemodel1]
        onStatusChanged: {
            if (status == RouteModel.Ready) {
                switch (count) {
                case 0:
                    clearAll() // technically not an error
                    map.routeError()
                    break
                case 1:
                    routeInfoModel.update()
                    break
                }
            } else if (status == RouteModel.Error) {
                clearAll()
                map.routeError()
            }
        }
//! [routemodel1]

//! [routemodel2]
        function clearAll() {
            routeInfoModel.update()
        }
//! [routemodel2]
//! [routemodel3]
    }
//! [routemodel3]

//! [geocodemodel0]
    property GeocodeModel geocodeModel: GeocodeModel {
//! [geocodemodel0]
//! [geocodemodel0 body]
        plugin: map.plugin
        onStatusChanged: {
            if ((status == GeocodeModel.Ready) || (status == GeocodeModel.Error))
                map.geocodeFinished()
        }
        onLocationsChanged:
        {
            if (count == 1) {
                map.center.latitude = get(0).coordinate.latitude
                map.center.longitude = get(0).coordinate.longitude
            }
        }
//! [geocodemodel0 body]
//! [geocodemodel1]
    }
//! [geocodemodel1]

    signal showGeocodeInfo()
    signal moveMarker()

    signal geocodeFinished()
    signal routeError()
    signal coordinatesCaptured(double latitude, double longitude)

    Component.onCompleted: {
        markers = new Array();
        mapItems = new Array();
    }

//! [routedelegate0]
    Component {
        id: routeDelegate

        MapRoute {
            route: routeData

            line.color: routeMouseArea.containsMouse ? "lime" : "red"
            line.width: 5
            smooth: true
            opacity: 0.8
//! [routedelegate0]
            MouseArea {
                id: routeMouseArea
                anchors.fill: parent
                hoverEnabled: false

                onPressed : {
                    map.resetState();
                    map.state = ""
                    map.lastX = mouse.x + parent.x
                    map.lastY = mouse.y + parent.y
                    map.pressX = mouse.x + parent.x
                    map.pressY = mouse.y + parent.y
                }

                onPositionChanged: {
                    if (map.state != "RoutePopupMenu" ||
                        Math.abs(map.pressX - parent.x- mouse.x ) > map.jitterThreshold ||
                        Math.abs(map.pressY - parent.y -mouse.y ) > map.jitterThreshold) {
                        map.state = ""
                    }
                    if ((mouse.button == Qt.LeftButton) & (map.state == "")) {
                        map.lastX = mouse.x + parent.x
                        map.lastY = mouse.y + parent.y
                    }
                }

                onPressAndHold:{
                    if (Math.abs(map.pressX - parent.x- mouse.x ) < map.jitterThreshold
                            && Math.abs(map.pressY - parent.y - mouse.y ) < map.jitterThreshold) {
                        map.state = "RoutePopupMenu"
                    }
                }
            }
        }
//! [routedelegate1]
    }
//! [routedelegate1]

//! [pointdel0]
    Component {
        id: pointDelegate

        MapCircle {
            radius: 1000
            color: circleMouseArea.containsMouse ? "lime" : "red"
            opacity: 0.6
            center: locationData.coordinate
//! [pointdel0]
            MouseArea {
                anchors.fill:parent
                id: circleMouseArea
                hoverEnabled: false

                onPressed : {
                    map.resetState();
                    map.state = ""
                    map.lastX = mouse.x + parent.x
                    map.lastY = mouse.y + parent.y
                    map.pressX = mouse.x + parent.x
                    map.pressY = mouse.y + parent.y
                }

                onPositionChanged: {
                    if (map.state != "PointPopupMenu" ||
                        Math.abs(map.pressX - parent.x- mouse.x ) > map.jitterThreshold ||
                        Math.abs(map.pressY - parent.y -mouse.y ) > map.jitterThreshold) {
                        map.state = ""
                        if (pressed) parent.radius = parent.center.distanceTo(
                                         map.toCoordinate(Qt.point(mouse.x, mouse.y)))
                    }
                    if ((mouse.button == Qt.LeftButton) & (map.state == "")) {
                        map.lastX = mouse.x + parent.x
                        map.lastY = mouse.y + parent.y
                    }
                }

                onPressAndHold:{
                    if (Math.abs(map.pressX - parent.x- mouse.x ) < map.jitterThreshold
                            && Math.abs(map.pressY - parent.y - mouse.y ) < map.jitterThreshold) {
                        map.state = "PointPopupMenu"
                    }
                }
            }
//! [pointdel1]
        }
    }
//! [pointdel1]

//! [routeinfodel0]
    Component {
        id: routeInfoDelegate
        Row {
            spacing: 10
//! [routeinfodel0]
            Text {
                id: indexText
                text: index + 1
                color: "#242424"
                font.bold: true
                font.pixelSize: 14
            }
//! [routeinfodel1]
            Text {
                id: instructionText
                text: instruction
                color: "#242424"
                wrapMode: Text.Wrap
//! [routeinfodel1]
                width: textArea.width - indexText.width - distanceText.width - spacing*4
//! [routeinfodel2]
                font.pixelSize: 14
            }
            Text {
                id: distanceText
                text: distance
                color: "#242424"
                font.bold: true
                font.pixelSize: 14
            }
        }
    }
//! [routeinfodel2]

    Component{
        id: routeInfoHeader
        Item {
            width: textArea.width
            height: travelTime.height + line.anchors.topMargin + line.height
            Text {
                id: travelTime
                text: routeInfoModel.travelTime
                color: "#242424"
                font.bold: true
                font.pixelSize: 14
                anchors.left: parent.left
            }
            Text {
                id: distance
                text: routeInfoModel.distance
                color: "#242424"
                font.bold: true
                font.pixelSize: 14
                anchors.right: parent.right
            }
            Rectangle {
                id: line
                color: "#242424"
                width: parent.width
                height: 2
                anchors.left: parent.left
                anchors.topMargin: 1
                anchors.top: distance.bottom
            }
        }
    }

//! [routeinfomodel]
    ListModel {
        id: routeInfoModel

        property string travelTime
        property string distance

        function update() {
            clear()
            if (routeModel.count > 0) {
                for (var i = 0; i < routeModel.get(0).segments.length; i++) {
                    append({
                        "instruction": routeModel.get(0).segments[i].maneuver.instructionText,
                        "distance": formatDistance(routeModel.get(0).segments[i].maneuver.distanceToNextInstruction)
                    });
                }
            }
            travelTime = routeModel.count == 0 ? "" : formatTime(routeModel.get(0).travelTime)
            distance = routeModel.count == 0 ? "" : formatDistance(routeModel.get(0).distance)
        }
    }
//! [routeinfomodel]

//! [routeview]
    MapItemView {
        model: routeModel
        delegate: routeDelegate
        autoFitViewport: true
    }
//! [routeview]

//! [geocodeview]
    MapItemView {
        model: geocodeModel
        delegate: pointDelegate
    }
//! [geocodeview]

    Item {
        id: infoTab
        parent: scale.parent
        z: map.z + 2
        height: parent.height - 180
        width: parent.width
        x: -5 - infoRect.width
        y: 60
        visible: (routeInfoModel.count > 0)
        Image {
            id: catchImage
            source: "../../resources/catch.png"
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (infoTab.x == -5) infoTab.x -= infoRect.width
                    else infoTab.x = -5
                    map.state = ""
                }
            }
        }

        Behavior on x {
            PropertyAnimation { properties: "x"; duration: 300; easing.type: Easing.InOutQuad }
        }

        Rectangle {
            id: infoRect
            width: parent.width - catchImage.sourceSize.width
            height: parent.height
            color: "#ECECEC"
            opacity: 1
            radius: 5
            MouseArea {
                anchors.fill: parent
                hoverEnabled: false
            }
            Item {
                id: textArea
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.top: parent.top
                anchors.topMargin: 10
                width: parent.width -15
                height: parent.height - 20
                ListView {
                    id: routeInfoView
                    model: routeInfoModel
                    delegate: routeInfoDelegate
                    header: routeInfoHeader
                    anchors.fill: parent
                    clip: true
                }
            }
        }
    }


    Item {//scale
        id: scale
        parent: zoomSlider.parent
        visible: scaleText.text != "0 m"
        z: map.z + 2
        opacity: 0.6
        anchors {
            bottom: zoomSlider.top;
            bottomMargin: 8;
            left: zoomSlider.left
        }
        Image {
            id: scaleImageLeft
            source: "../../resources/scale_end.png"
            anchors.bottom: parent.bottom
            anchors.left: parent.left
        }
        Image {
            id: scaleImage
            source: "../../resources/scale.png"
            anchors.bottom: parent.bottom
            anchors.left: scaleImageLeft.right
        }
        Image {
            id: scaleImageRight
            source: "../../resources/scale_end.png"
            anchors.bottom: parent.bottom
            anchors.left: scaleImage.right
        }
        Text {
            id: scaleText
            color: "#004EAE"
            horizontalAlignment: Text.AlignHCenter
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.bottomMargin: 3
            text: "0 m"
            font.pixelSize: 14
        }
        Component.onCompleted: {
            map.calculateScale();
        }
    }

    Timer {
        id: scaleTimer
        interval: 100
        running: false
        repeat: false
        onTriggered: {
            map.calculateScale()
        }
    }

    onCenterChanged:{
        scaleTimer.restart()
        if (map.followme)
            if (map.center != positionSource.position.coordinate) map.followme = false
    }

    onZoomLevelChanged:{
        scaleTimer.restart()
        if (map.followme) map.center = positionSource.position.coordinate
    }

    onWidthChanged:{
        scaleTimer.restart()
    }

    onHeightChanged:{
        scaleTimer.restart()
    }

    Menu {
        id: markerMenu
        horizontalOrientation: false
        autoWidth: true
        z: map.z + 4
        opacity: 0

        width: 150
        x: 0
        y: 0
        onClicked: {
            map.state = ""
            switch (button) {
                case "Delete": {//remove marker
                    map.deleteMarker(currentMarker)
                    break;
                }
                case "Move to": {//move marker
                    map.moveMarker()
                    break;
                }
                case "Coordinates": {//show marker's coordinates
                    map.coordinatesCaptured(markers[currentMarker].coordinate.latitude, markers[currentMarker].coordinate.longitude)
                    break;
                }
                case "Distance to next point": {
                    showDistance(formatDistance(map.markers[currentMarker].coordinate.distanceTo(map.markers[currentMarker+1].coordinate)));
                    break;
                }
                case "Route to next points"://calculate route
                case "Route to next point": {
                    map.calculateRoute()
                    break;
                }
                case "Draw...": {
                    map.drawItemPopup()
                    break;
                }
            }
        }
    }

    Menu {
        id: drawMenu
        horizontalOrientation: false
        autoWidth: true
        z: map.z + 4
        opacity: 0

        width: 150
        x: 0
        y: 0
        onClicked: {
            map.state = ""
            switch (button) {
            case "Polyline": {
                addGeoItem("PolylineItem")
                break;
            }

            case "Rectangle": {
                addGeoItem("RectangleItem")
                break;
            }

            case "Circle": {
                addGeoItem("CircleItem")
                break;
            }

            case "Polygon": {
                addGeoItem("PolygonItem")
                break;
            }

            case "Image": {
                addGeoItem("ImageItem")
                break;
            }

            case "Video": {
                addGeoItem("VideoItem")
                break;
            }

            case "3D QML Item": {
                addGeoItem("3dItem")
                break;
            }
            }
        }
    }

    Menu {
        id: popupMenu
        horizontalOrientation: false
        autoWidth: true
        z: map.z + 4
        opacity: 0

        width: 150
        x: 0
        y: 0

        onClicked: {
            switch (button) {
            case "Add Marker": {
                addMarker()
                break;
            }
            case "Get coordinate": {
                map.coordinatesCaptured(mouseArea.lastCoordinate.latitude, mouseArea.lastCoordinate.longitude)
                break;
            }
            case "Fit Viewport To Map Items": {
                map.fitViewportToMapItems()
                break;
            }

            case "Delete all markers": {
                deleteMarkers()
                break;
            }

            case "Delete all items": {
                deleteMapItems()
                break;
            }
            }
            map.state = ""
        }
    }

    Menu {
        id: routeMenu
        horizontalOrientation: false
        autoWidth: true
        z: map.z + 4
        opacity: 0

        width: 150
        x: 0
        y: 0

        onClicked: {
            switch (button) {
                case "Delete": {//delete route
                    routeModel.reset()
                    routeInfoModel.update()
                    break;
                }
            }
            map.state = ""
        }
        Component.onCompleted: {
            addItem("Delete")
        }
    }

    Menu {
        id: pointMenu
        horizontalOrientation: false
        autoWidth: true
        z: map.z + 4
        opacity: 0

        width: 150
        x: 0
        y: 0

        onClicked: {
            switch (button) {
                case "Info": {
                    map.showGeocodeInfo()
                    break;
                }
                case "Delete": {
                    geocodeModel.reset()
                    break;
                }
            }
            map.state = ""
        }
        Component.onCompleted: {
            addItem("Info")
            addItem("Delete")
        }
    }

    Rectangle {
        id: infoLabel
        width: backgroundRect.width + 10
        height: infoText.height + 5
        y: 440
        anchors.left: map.left
        z: map.z + 1
        color: "dimgrey"
        opacity: (infoText.text !="") ? 0.8 : 0

        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }
        Text {
            id: infoText
            width: parent.width
            elide: Text.ElideLeft
            maximumLineCount: 4
            wrapMode: Text.Wrap
            font.bold: true
            font.pixelSize: 14
            style: Text.Raised;
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 5
            anchors.rightMargin: 5
            color: "white"
        }
    }

    MouseArea {
        id: mouseArea
        property variant lastCoordinate
        anchors.fill: parent

        onPressed : {
            map.resetState();
            map.state = ""
            map.lastX = mouse.x
            map.lastY = mouse.y
            map.pressX = mouse.x
            map.pressY = mouse.y
            lastCoordinate = map.toCoordinate(Qt.point(mouse.x, mouse.y))
            //            if (mouse.button == Qt.MiddleButton)
            //                addMarker()
        }

        onPositionChanged: {
            if (map.state != "PopupMenu" ||
                Math.abs(map.pressX - mouse.x ) > map.jitterThreshold ||
                Math.abs(map.pressY - mouse.y ) > map.jitterThreshold) {
                map.state = ""
            }
            if ((mouse.button == Qt.LeftButton) & (map.state == "")) {
                //                if ((map.lastX != -1) && (map.lastY != -1)) {
                //                    var dx = mouse.x - map.lastX
                //                    var dy = mouse.y - map.lastY
                //                    map.pan(-dx, -dy)
                //                }
                map.lastX = mouse.x
                map.lastY = mouse.y
            }
        }

        onDoubleClicked: {
            map.center = map.toCoordinate(Qt.point(mouse.x, mouse.y))
            if (mouse.button == Qt.LeftButton){
                map.zoomLevel += 1
            } else if (mouse.button == Qt.RightButton) map.zoomLevel -= 1
            lastX = -1
            lastY = -1
        }

        onPressAndHold:{
            if (Math.abs(map.pressX - mouse.x ) < map.jitterThreshold
                    && Math.abs(map.pressY - mouse.y ) < map.jitterThreshold) {
                popupMenu.clear()
                popupMenu.addItem("Add Marker")
                popupMenu.addItem("Get coordinate")
                popupMenu.addItem("Fit Viewport To Map Items")

                if (map.markers.length>0) {
                    popupMenu.addItem("Delete all markers")
                }

                if (map.mapItems.length>0) {
                    popupMenu.addItem("Delete all items")
                }
                map.state = "PopupMenu"
            }
          }
        }


    Keys.onPressed: {
        if ((event.key == Qt.Key_Plus) || (event.key == Qt.Key_VolumeUp)) {
            map.zoomLevel += 1
        } else if ((event.key == Qt.Key_Minus) || (event.key == Qt.Key_VolumeDown)){
            map.zoomLevel -= 1
        }
    }

    function calculateScale(){
        var coord1, coord2, dist, text, f
        f = 0
        coord1 = map.toCoordinate(Qt.point(0,scale.y))
        coord2 = map.toCoordinate(Qt.point(0+scaleImage.sourceSize.width,scale.y))
        dist = Math.round(coord1.distanceTo(coord2))

        if (dist === 0) {
            // not visible
        } else {
            for (var i = 0; i < scaleLengths.length-1; i++) {
                if (dist < (scaleLengths[i] + scaleLengths[i+1]) / 2 ) {
                    f = scaleLengths[i] / dist
                    dist = scaleLengths[i]
                    break;
                }
            }
            if (f === 0) {
                f = dist / scaleLengths[i]
                dist = scaleLengths[i]
            }
        }

        text = formatDistance(dist)
        scaleImage.width = (scaleImage.sourceSize.width * f) - 2 * scaleImageLeft.sourceSize.width
        scaleText.text = text
    }


    function deleteMarkers(){
        var count = map.markers.length
        for (var i = 0; i<count; i++){
            map.removeMapItem(map.markers[i])
            map.markers[i].destroy()
        }
        map.markers = []
        markerCounter = 0
    }

    function deleteMapItems(){
        var count = map.mapItems.length
        for (var i = 0; i<count; i++){
            map.removeMapItem(map.mapItems[i])
            map.mapItems[i].destroy()
        }
        map.mapItems = []
    }

    function addMarker(){
        var count = map.markers.length
        markerCounter++
        var marker = Qt.createQmlObject ('Marker {}', map)
        map.addMapItem(marker)
        marker.z = map.z+1
        marker.coordinate = mouseArea.lastCoordinate

        //update list of markers
        var myArray = new Array()
        for (var i = 0; i<count; i++){
            myArray.push(markers[i])
        }
        myArray.push(marker)
        markers = myArray
    }

    function addGeoItem(item){
        var count = map.mapItems.length
        var co = Qt.createComponent(item+'.qml')
        if (co.status == Component.Ready) {
            var o = co.createObject(map)
            o.setGeometry(map.markers, currentMarker)
            map.addMapItem(o)
            //update list of items
            var myArray = new Array()
            for (var i = 0; i<count; i++){
                myArray.push(mapItems[i])
            }
            myArray.push(o)
            mapItems = myArray

        } else {
            console.log(item + " is not supported right now, please call us later.")
        }
    }

    function deleteMarker(index){
        //update list of markers
        var myArray = new Array()
        var count = map.markers.length
        for (var i = 0; i<count; i++){
            if (index != i) myArray.push(map.markers[i])
        }

        map.removeMapItem(map.markers[index])
        map.markers[index].destroy()
        map.markers = myArray
        if (markers.length == 0) markerCounter = 0
    }

    function markerPopup(){
        var array
        var length = map.markers.length

        markerMenu.clear()
        markerMenu.addItem("Delete")
        markerMenu.addItem("Coordinates")
        markerMenu.addItem("Move to")
        markerMenu.addItem("Draw...")


        if (currentMarker == length-2){
            markerMenu.addItem("Route to next point")
            markerMenu.addItem("Distance to next point")

        }
        if (currentMarker < length-2){
            markerMenu.addItem("Route to next points")
            markerMenu.addItem("Distance to next point")
        }
        map.state = "MarkerPopupMenu"
    }


    function drawItemPopup(){
        var array
        var length = map.markers.length

        drawMenu.clear()

        drawMenu.addItem("Image")
        drawMenu.addItem("Video")
        drawMenu.addItem("3D QML Item")

        if (currentMarker <= length-2){
            drawMenu.addItem("Rectangle")
            drawMenu.addItem("Circle")
            drawMenu.addItem("Polyline")
        }
        if (currentMarker < length-2){
            drawMenu.addItem("Polygon")
        }
        map.state = "DrawItemMenu"
    }

    function calculateRoute(){
        routeQuery.clearWaypoints();
        for (var i = currentMarker; i< map.markers.length; i++){
            routeQuery.addWaypoint(markers[i].coordinate)
        }
        routeQuery.travelModes = RouteQuery.CarTravel
        routeQuery.routeOptimizations = RouteQuery.ShortestRoute
        routeQuery.setFeatureWeight(0, 0)
        routeModel.update();
    }

    function roundNumber(number, digits) {
        var multiple = Math.pow(10, digits);
        return Math.round(number * multiple) / multiple;
    }

    function formatTime(sec){
        var value = sec
        var seconds = value % 60
        value /= 60
        value = (value > 1) ? Math.round(value) : 0
        var minutes = value % 60
        value /= 60
        value = (value > 1) ? Math.round(value) : 0
        var hours = value
        if (hours > 0) value = hours + "h:"+ minutes + "m"
        else value = minutes + "min"
        return value
    }

    function formatDistance(meters)
    {
        var dist = Math.round(meters)
        if (dist > 1000 ){
            if (dist > 100000){
                dist = Math.round(dist / 1000)
            }
            else{
                dist = Math.round(dist / 100)
                dist = dist / 10
            }
            dist = dist + " km"
        }
        else{
            dist = dist + " m"
        }
        return dist
    }

    function setLanguage(lang) {
        map.plugin.locales = lang;
        if (map.plugin.locales.length  >  0) {
            languageButton.text = map.plugin.locales[0];
        }
    }

    // states of map
    states: [
        State {
            name: "PopupMenu"
            PropertyChanges { target: popupMenu; opacity: 1}
            PropertyChanges { target: popupMenu; x: ((map.lastX + popupMenu.width > map.width) ? map.width - popupMenu.width : map.lastX)}
            PropertyChanges { target: popupMenu; y: ((map.lastY + popupMenu.height > map.height - 40) ? map.height - popupMenu.height - 40 : map.lastY)}
        },
        State {
            name: "MarkerPopupMenu"
            PropertyChanges { target: markerMenu; opacity: 1}
            PropertyChanges { target: markerMenu; x: ((markers[currentMarker].lastMouseX + markerMenu.width > map.width) ? map.width - markerMenu.width : markers[currentMarker].lastMouseX )}
            PropertyChanges { target: markerMenu; y: ((markers[currentMarker].lastMouseY + markerMenu.height > map.height - 40) ? map.height - markerMenu.height - 40 : markers[currentMarker].lastMouseY)}
        },
        State {
            name: "DrawItemMenu"
            PropertyChanges { target: drawMenu; opacity: 1}
            PropertyChanges { target: drawMenu; x: ((markers[currentMarker].lastMouseX + drawMenu.width > map.width) ? map.width - drawMenu.width : markers[currentMarker].lastMouseX )}
            PropertyChanges { target: drawMenu; y: ((markers[currentMarker].lastMouseY + drawMenu.height > map.height - 40) ? map.height - drawMenu.height - 40 : markers[currentMarker].lastMouseY)}
        },
        State {
            name: "RoutePopupMenu"
            PropertyChanges { target: routeMenu; opacity: 1}
            PropertyChanges { target: routeMenu; x: ((map.lastX + routeMenu.width > map.width) ? map.width - routeMenu.width : map.lastX)}
            PropertyChanges { target: routeMenu; y: ((map.lastY + routeMenu.height > map.height - 40) ? map.height - routeMenu.height - 40 : map.lastY)}
        },
        State {
            name: "PointPopupMenu"
            PropertyChanges { target: pointMenu; opacity: 1}
            PropertyChanges { target: pointMenu; x: ((map.lastX + pointMenu.width > map.width) ? map.width - pointMenu.width : map.lastX)}
            PropertyChanges { target: pointMenu; y: ((map.lastY + pointMenu.height > map.height - 40) ? map.height - pointMenu.height - 40 : map.lastY)}
        },
        State {
            name: "LanguageMenu"
            PropertyChanges { target: languageMenu; opacity: 1}
        }
    ]
//! [end]
}
//! [end]
