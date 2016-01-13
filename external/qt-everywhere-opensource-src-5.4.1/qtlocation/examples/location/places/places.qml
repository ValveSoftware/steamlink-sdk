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
import QtLocation.examples 5.0
import "content/places"

Item {
    id: page
    width: (parent && parent.width > 0) ? parent.width : 360
    height: (parent && parent.height > 0) ? parent.height : 640
    property variant map
    property variant startLocation
    property variant searchRegion: QtPositioning.circle(startLocation)
    property variant searchRegionItem
    property Plugin favoritesPlugin

    onMapChanged: editPlaceDialog.prepareDialog()

    Binding {
        target: page
        property: "startLocation"
        value: map ? map.center : QtPositioning.coordinate()
    }

    Rectangle {
        id: backgroundRect
        anchors.fill: parent
        color: "lightgrey"
        z: 2
    }

    //=====================Menu=====================
    Menu {
        id:mainMenu
        anchors.bottom: parent.bottom
        z: backgroundRect.z + 3

        Component.onCompleted: {
            addItem("Provider");
            addItem("New");
            addItem("Search");
        }

        onClicked: page.state = page.state == "" ? button : "";
    }

    Menu {
        id: providerMenu
        z: backgroundRect.z + 2
        y: page.height
        horizontalOrientation: false
        exclusive: true

        Component.onCompleted: {
            var plugins = getPlacesPlugins()
            for (var i = 0; i<plugins.length; i++) {
                addItem(plugins[i]);

                // default to nokia plugin
                if (plugins[i] === "nokia")
                    exclusiveButton = plugins[i];
            }

            // otherwise default to first
            if (exclusiveButton === "")
                exclusiveButton = plugins[0];
        }

        onClicked: page.state = ""

        onExclusiveButtonChanged: placesPlugin.name = exclusiveButton;
    }

    Menu {
        id: newMenu
        z: backgroundRect.z + 2
        y: page.height
        horizontalOrientation: false

        Component.onCompleted: {
            var item = addItem("Place");
            item.enabled = Qt.binding(function() { return placesPlugin.name != "" ? placesPlugin.supportsPlaces(Plugin.SavePlaceFeature) : false })

            item = addItem("Category");
            item.enabled = Qt.binding(function() { return placesPlugin.name != "" ? placesPlugin.supportsPlaces(Plugin.SaveCategoryFeature) : false })
        }

        onClicked: {
            switch (button) {
            case "Place": {
                editPlaceDialog.prepareDialog();

                page.state = "NewPlace";
                break;
            }
            case "Category": {
                editCategoryDialog.category = null;
                editCategoryDialog.prepareDialog();
                page.state = "NewCategory";
                break;
            }
            }
        }
    }

    Menu {
        id: searchMenu
        z: backgroundRect.z + 2
        y: page.height
        horizontalOrientation: false

        Component.onCompleted: {
            addItem("Search Center");
            addItem("Search Bounding Box");
            addItem("Search Bounding Circle");
            addItem("Search Options");
        }

        onClicked: page.state = button
    }

    //=====================Dialogs=====================
    PlaceDialog {
        id: editPlaceDialog
        z: backgroundRect.z + 4

        onCancelButtonClicked: page.state = ""
        onCompleted: page.state = "";
    }

    CategoryDialog {
        id: editCategoryDialog
        z: backgroundRect.z + 4

        onCancelButtonClicked: page.state = ""

        Connections {
            target: editCategoryDialog.category
            onStatusChanged: {
                switch (editCategoryDialog.category.status) {
                case Category.Saving: {
                    break;
                }
                case Category.Ready: {
                    page.state = "";
                    break;
                }
                case Category.Error: {
                    console.log("Error while saving!");
                    break;
                }
                }
            }
        }
    }

    InputDialog {
        id: searchCenterDialog
        z: backgroundRect.z + 4

        title: "Search center"

        Behavior on opacity { NumberAnimation { duration: 500 } }

        Component.onCompleted: prepareDialog()

        function prepareDialog() {
            setModel([
                ["Latitude", searchRegion.center ? String(searchRegion.center.latitude) : ""],
                ["Longitude", searchRegion.center ? String(searchRegion.center.longitude) : ""]
            ]);
        }

        onCancelButtonClicked: page.state = ""
        onGoButtonClicked: {
            var c = QtPositioning.coordinate(parseFloat(dialogModel.get(0).inputText),
                                          parseFloat(dialogModel.get(1).inputText));

            map.center = c;

            searchRegion = Qt.binding(function() { return QtPositioning.circle(startLocation) });

            if (searchRegionItem) {
                map.removeMapItem(searchRegionItem);
                searchRegionItem.destroy();
            }

            page.state = "";
        }
    }

    InputDialog {
        id: searchBoxDialog
        z: backgroundRect.z + 4

        title: "Search Bounding Box"

        Behavior on opacity { NumberAnimation { duration: 500 } }

        Component.onCompleted: prepareDialog()

        function prepareDialog() {
            setModel([
                ["Latitude", searchRegion.center ? String(searchRegion.center.latitude) : ""],
                ["Longitude", searchRegion.center ? String(searchRegion.center.longitude) : ""],
                ["Width(deg)", searchRegion.width ? String(searchRegion.width) : "" ],
                ["Height(deg)", searchRegion.height ? String(searchRegion.height) : "" ]
            ]);
        }

        onCancelButtonClicked: page.state = ""
        onGoButtonClicked: {
            var c = QtPositioning.coordinate(parseFloat(dialogModel.get(0).inputText),
                                          parseFloat(dialogModel.get(1).inputText));
            var r = QtPositioning.rectangle(c, parseFloat(dialogModel.get(2).inputText),
                                         parseFloat(dialogModel.get(3).inputText));

            map.center = c;

            searchRegion = r;

            if (searchRegionItem) {
                map.removeMapItem(searchRegionItem);
                searchRegionItem.destroy();
            }

            searchRegionItem = Qt.createQmlObject('import QtLocation 5.3; MapRectangle { color: "red"; opacity: 0.4 }', page, "MapRectangle");
            searchRegionItem.topLeft = r.topLeft;
            searchRegionItem.bottomRight = r.bottomRight;
            map.addMapItem(searchRegionItem);

            page.state = "";
        }
    }

    InputDialog {
        id: searchCircleDialog
        z: backgroundRect.z + 4

        title: "Search Bounding Circle"

        Behavior on opacity { NumberAnimation { duration: 500 } }

        Component.onCompleted: prepareDialog()

        function prepareDialog() {
            setModel([
                ["Latitude", searchRegion.center ? String(searchRegion.center.latitude) : ""],
                ["Longitude", searchRegion.center ? String(searchRegion.center.longitude) : ""],
                ["Radius(m)", searchRegion.radius ? String(searchRegion.radius) : "" ]
            ]);
        }

        onCancelButtonClicked: page.state = ""
        onGoButtonClicked: {
            var c = QtPositioning.coordinate(parseFloat(dialogModel.get(0).inputText),
                                          parseFloat(dialogModel.get(1).inputText));
            var circle = QtPositioning.circle(c, parseFloat(dialogModel.get(2).inputText));

            map.center = c;

            searchRegion = circle;

            if (searchRegionItem) {
                map.removeMapItem(searchRegionItem);
                searchRegionItem.destroy();
            }

            searchRegionItem = Qt.createQmlObject('import QtLocation 5.3; MapCircle { color: "red"; opacity: 0.4 }', page, "MapRectangle");
            searchRegionItem.center = circle.center;
            searchRegionItem.radius = circle.radius;
            map.addMapItem(searchRegionItem);

            page.state = "";
        }
    }

    OptionsDialog {
        id: optionsDialog
        z: backgroundRect.z + 4

        Behavior on opacity { NumberAnimation { duration: 500 } }

        Component.onCompleted: prepareDialog()

        function prepareDialog() {
            if (placeSearchModel.favoritesPlugin !== null)
                isFavoritesEnabled = true;
            else
                isFavoritesEnabled = false;

            locales = placesPlugin.locales.join(Qt.locale().groupSeparator);
        }

        onCancelButtonClicked: page.state = ""
        onGoButtonClicked: {
            /*if (isFavoritesEnabled) {
                if (favoritesPlugin == null)
                    favoritesPlugin = Qt.createQmlObject('import QtLocation 5.3; Plugin { name: "places_jsondb" }', page);
                favoritesPlugin.parameters = pluginParametersFromMap(pluginParameters);
                placeSearchModel.favoritesPlugin = favoritesPlugin;
            } else {
                placeSearchModel.favoritesPlugin = null;
            }*/
            placeSearchModel.favoritesPlugin = null;

            placeSearchModel.relevanceHint = orderByDistance ? PlaceSearchModel.DistanceHint :
                                                               orderByName ? PlaceSearchModel.LexicalPlaceNameHint :
                                                                             PlaceSearchModel.UnspecifiedHint;
            placesPlugin.locales = locales.split(Qt.locale().groupSeparator);
            categoryModel.update();
            page.state = "";
        }
    }

    //! [PlaceSearchModel model]
    PlaceSearchModel {
        id: placeSearchModel

        plugin: placesPlugin
        searchArea: searchRegion

        function searchForCategory(category) {
            searchTerm = "";
            categories = category;
            recommendationId = "";
            searchArea = searchRegion
            limit = -1;
            update();
        }

        function searchForText(text) {
            searchTerm = text;
            categories = null;
            recommendationId = "";
            searchArea = searchRegion
            limit = -1;
            update();
        }

        function searchForRecommendations(placeId) {
            searchTerm = "";
            categories = null;
            recommendationId = placeId;
            searchArea = null;
            limit = -1;
            update();
        }

        onStatusChanged: {
            switch (status) {
            case PlaceSearchModel.Ready:
                searchResultView.showSearchResults();
                break;
            case PlaceSearchModel.Error:
                console.log(errorString());
            }
        }
    }
    //! [PlaceSearchModel model]

    //! [CategoryModel model]
    CategoryModel {
        id: categoryModel
        plugin: placesPlugin
        hierarchical: true
    }
    //! [CategoryModel model]

    SearchBox {
        id: searchBox

        anchors.top: page.top
        width: parent.width
        expandedHeight: parent.height
        z: backgroundRect.z + 3
    }

    Plugin {
        id: placesPlugin

        parameters: pluginParametersFromMap(pluginParameters)
        onNameChanged: {
            createMap(placesPlugin);
            categoryModel.update();
        }
    }

    Item {
        id: searchResultTab

        z: backgroundRect.z + 2
        height: parent.height - searchBox.baseHeight - mainMenu.height
        width: parent.width
        x: 0
        y: mainMenu.height - height + catchImage.width

        opacity: 0

        property bool open: false

        Behavior on y { PropertyAnimation { duration: 300; easing.type: Easing.InOutQuad } }
        Behavior on opacity { PropertyAnimation { duration: 300 } }

        Image {
            id: catchImage

            source: "resources/catch.png"
            rotation: 90
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: (width - height) / 2

            MouseArea {
                anchors.fill: parent
                onClicked: searchResultTab.open = !searchResultTab.open;
            }
        }

        Rectangle {
            id: searchResultTabPage

            width: parent.width
            height: parent.height - catchImage.width
            color: "#ECECEC"
            radius: 5

            SearchResultView {
                id: searchResultView

                anchors.fill: parent
                anchors.margins: 10
            }
        }

        states: [
            State {
                name: ""
                when: placeSearchModel.count == 0
                PropertyChanges { target: searchResultTab; open: false }
            },
            State {
                name: "Close"
                when: (placeSearchModel.count > 0) && !searchResultTab.open
                PropertyChanges { target: searchResultTab; opacity: 1 }
            },
            State {
                name: "Open"
                when: (placeSearchModel.count > 0) && searchResultTab.open
                PropertyChanges { target: searchResultTab; y: mainMenu.height; opacity: 1 }
            }
        ]
    }

    Component {
        id: mapComponent

        MapComponent {
            z: backgroundRect.z + 1
            width: page.width
            height: page.height - mainMenu.height

            MapItemView {
                model: placeSearchModel
                delegate: MapQuickItem {
                    coordinate: model.type === PlaceSearchModel.PlaceResult ? place.location.coordinate : QtPositioning.coordinate()

                    visible: model.type === PlaceSearchModel.PlaceResult

                    anchorPoint.x: image.width * 0.28
                    anchorPoint.y: image.height

                    sourceItem: Image {
                        id: image

                        source: "resources/marker.png"

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                searchResultView.showPlaceDetails({
                                    distance: model.distance,
                                    place: model.place,
                                });
                                searchResultTab.state = "Open";
                            }
                        }
                    }
                }
            }
        }
    }

    function createMap(placesPlugin) {
        var mapPlugin;
        if (placesPlugin.supportsMapping()) {
            mapPlugin = placesPlugin;
        } else {
            mapPlugin = Qt.createQmlObject('import QtLocation 5.3; Plugin { required.mapping: Plugin.AnyMappingFeatures;' +
                                                                           'parameters: pluginParametersFromMap(pluginParameters) }', page);
        }

        if (map)
            map.destroy();
        map = mapComponent.createObject(page);
        map.plugin = mapPlugin;
    }

    function getPlacesPlugins() {
        var plugin = Qt.createQmlObject('import QtLocation 5.3; Plugin {}', page);
        var myArray = new Array;
        for (var i = 0; i < plugin.availableServiceProviders.length; i++) {
            var tempPlugin = Qt.createQmlObject ('import QtLocation 5.3; Plugin {name: "' + plugin.availableServiceProviders[i]+ '"}', page)

            if (tempPlugin.supportsPlaces())
                myArray.push(tempPlugin.name)
        }

        return myArray;
    }

    function pluginParametersFromMap(pluginParameters) {
        var parameters = new Array()
        for (var prop in pluginParameters){
            var parameter = Qt.createQmlObject('import QtLocation 5.3; PluginParameter{ name: "'+ prop + '"; value: "' + pluginParameters[prop]+'"}',page)
            parameters.push(parameter)
        }
        return parameters
        //createMap(placesPlugin)
    }

    //=====================States of page=====================
    states: [
        State {
            name: "Provider"
            PropertyChanges { target: providerMenu; y: page.height - providerMenu.height - mainMenu.height }
        },
        State {
            name: "New"
            PropertyChanges { target: newMenu; y: page.height - newMenu.height - mainMenu.height }
        },
        State {
            name: "Search"
            PropertyChanges { target: searchMenu; y: page.height - searchMenu.height - mainMenu.height }
        },
        State {
            name: "NewPlace"
            PropertyChanges { target: editPlaceDialog; title: "New Place"; opacity: 1 }
        },
        State {
            name: "NewCategory"
            PropertyChanges { target: editCategoryDialog; title: "New Category"; opacity: 1 }
        },
        State {
            name: "EditPlace"
            PropertyChanges { target: editPlaceDialog; title: "Edit Place"; opacity: 1 }
        },
        State {
            name: "EditCategory"
            PropertyChanges { target: editCategoryDialog; opacity: 1 }
        },
        State {
            name: "Search Center"
            PropertyChanges { target: searchCenterDialog; opacity: 1 }
            StateChangeScript { script: searchCenterDialog.prepareDialog() }
        },
        State {
            name: "Search Bounding Box"
            PropertyChanges { target: searchBoxDialog; opacity: 1 }
            StateChangeScript { script: searchBoxDialog.prepareDialog() }
        },
        State {
            name: "Search Bounding Circle"
            PropertyChanges { target: searchCircleDialog; opacity: 1 }
            StateChangeScript { script: searchCircleDialog.prepareDialog() }
        },
        State {
            name: "Search Options"
            PropertyChanges { target: optionsDialog; opacity: 1 }
            StateChangeScript { script: optionsDialog.prepareDialog() }
        }
    ]

    //=====================State-transition animations for page=====================
    transitions: [
        Transition {
            to: ""
            NumberAnimation { properties: "opacity,y,x,rotation" ; duration: 500; easing.type: Easing.Linear }
        },
        Transition {
            to: "Provider"
            NumberAnimation { properties: "y" ; duration: 300; easing.type: Easing.Linear }
        },
        Transition {
            to: "New"
            NumberAnimation { properties: "y" ; duration: 300; easing.type: Easing.Linear }
        },
        Transition {
            to: "Search"
            NumberAnimation { properties: "y" ; duration: 300; easing.type: Easing.Linear }
        }
    ]
}
