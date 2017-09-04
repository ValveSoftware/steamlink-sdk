/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
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

import QtQuick 2.5
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.2
import QtPositioning 5.5
import QtLocation 5.6
import "items"

ApplicationWindow {
    id: appWindow
    property Map map
    property variant parameters
    property variant searchLocation: map ? map.center : QtPositioning.coordinate()
    property variant searchRegion: QtPositioning.circle(searchLocation)
    property variant searchRegionItem

    property Plugin favoritesPlugin

    function getPlugins() {
        var plugin = Qt.createQmlObject('import QtLocation 5.3; Plugin {}', appWindow);
        var myArray = new Array;
        for (var i = 0; i < plugin.availableServiceProviders.length; i++) {
            var tempPlugin = Qt.createQmlObject ('import QtLocation 5.3; Plugin {name: "' + plugin.availableServiceProviders[i]+ '"}', appWindow)

            if (tempPlugin.supportsPlaces() && tempPlugin.supportsMapping() )
                myArray.push(tempPlugin.name)
        }
        myArray.sort()
        return myArray;
    }

    function initializeProviders(pluginParameters)
    {
        var parameters = new Array()
        for (var prop in pluginParameters) {
            var parameter = Qt.createQmlObject('import QtLocation 5.3; PluginParameter{ name: "'+ prop + '"; value: "' + pluginParameters[prop]+'"}',appWindow)
            parameters.push(parameter)
        }
        appWindow.parameters = parameters
        var plugins = getPlugins()
        mainMenu.providerMenu.createMenu(plugins)
        for (var i = 0; i<plugins.length; i++) {
            if (plugins[i] === "osm")
                mainMenu.selectProvider(plugins[i])
        }
    }

    function createMap(provider) {
        var plugin;
        if (parameters && parameters.length>0)
            plugin = Qt.createQmlObject ('import QtLocation 5.3; Plugin{ name:"' + provider + '"; parameters: appWindow.parameters}', appWindow)
        else
            plugin = Qt.createQmlObject ('import QtLocation 5.3; Plugin{ name:"' + provider + '"}', appWindow)

        if (map)
            map.destroy();
        map = mapComponent.createObject(page);
        map.plugin = plugin;
        map.zoomLevel = (map.maximumZoomLevel - map.minimumZoomLevel)/2
        categoryModel.plugin = plugin;
        categoryModel.update();
        placeSearchModel.plugin = plugin;
        suggestionModel.plugin = plugin;
    }

    title: qsTr("Places")
    width: 360
    height: 640
    visible: true
    menuBar: mainMenu
    toolBar: searchBar

    MainMenu {
        id: mainMenu
        onSelectProvider: {
            stackView.pop(page)
            for (var i = 0; i < providerMenu.items.length; i++) {
                providerMenu.items[i].checked = providerMenu.items[i].text === providerName
            }

            createMap(providerName)
            if (map.error === Map.NoError) {
                settingsMenu.createMenu(map);
            } else {
                settingsMenu.clear();
            }
        }
        onSelectSetting: {
            stackView.pop({tem:page,immediate: true})
            switch (setting) {
            case "searchCenter":
                stackView.push({ item: Qt.resolvedUrl("forms/SearchCenter.qml") ,
                                   properties: { "coordinate": map.center}})
                stackView.currentItem.changeSearchCenter.connect(stackView.changeSearchCenter)
                stackView.currentItem.closeForm.connect(stackView.closeForm)
                break
            case "searchBoundingBox":
                stackView.push({ item: Qt.resolvedUrl("forms/SearchBoundingBox.qml") ,
                                   properties: { "searchRegion": searchRegion}})
                stackView.currentItem.changeSearchBoundingBox.connect(stackView.changeSearchBoundingBox)
                stackView.currentItem.closeForm.connect(stackView.closeForm)
                break
            case "searchBoundingCircle":
                stackView.push({ item: Qt.resolvedUrl("forms/SearchBoundingCircle.qml") ,
                                   properties: { "searchRegion": searchRegion}})
                stackView.currentItem.changeSearchBoundingCircle.connect(stackView.changeSearchBoundingCircle)
                stackView.currentItem.closeForm.connect(stackView.closeForm)
                break
            case "SearchOptions":
                stackView.push({ item: Qt.resolvedUrl("forms/SearchOptions.qml") ,
                                   properties: { "plugin": map.plugin,
                                       "model": placeSearchModel}})
                stackView.currentItem.changeSearchSettings.connect(stackView.changeSearchSettings)
                stackView.currentItem.closeForm.connect(stackView.closeForm)
                break
            default:
                console.log("Unsupported setting !")
            }
        }
    }

    //! [PlaceSearchSuggestionModel search text changed 1]
    SearchBar {
        id: searchBar
    //! [PlaceSearchSuggestionModel search text changed 1]
        width: appWindow.width
        searchBarVisbile: stackView.depth > 1 &&
                          stackView.currentItem &&
                          stackView.currentItem.objectName != "suggestionView" ? false : true
        onShowCategories: {
            if (map && map.plugin) {
                stackView.pop({tem:page,immediate: true})
                stackView.enterCategory()
            }
        }
        onGoBack: stackView.pop()
    //! [PlaceSearchSuggestionModel search text changed 2]
        onSearchTextChanged: {
            if (searchText.length >= 3 && suggestionModel != null) {
                suggestionModel.searchTerm = searchText;
                suggestionModel.update();
            }
        }
    //! [PlaceSearchSuggestionModel search text changed 2]
        onDoSearch: {
            if (searchText.length > 0)
                placeSearchModel.searchForText(searchText);
        }
        onShowMap: stackView.pop(page)
    //! [PlaceSearchSuggestionModel search text changed 3]
    }
    //! [PlaceSearchSuggestionModel search text changed 3]

    StackView {
        id: stackView

        function showMessage(title,message,backPage)
        {
            push({ item: Qt.resolvedUrl("forms/Message.qml") ,
                     properties: {
                         "title" : title,
                         "message" : message,
                         "backPage" : backPage
                     }})
            currentItem.closeForm.connect(closeMessage)
        }

        function closeMessage(backPage)
        {
            pop(backPage)
        }

        function closeForm()
        {
            pop(page)
        }

        function enterCategory(index)
        {
            push({ item: Qt.resolvedUrl("views/CategoryView.qml") ,
                     properties: { "categoryModel": categoryModel,
                         "rootIndex" : index
                     }})
            currentItem.showSubcategories.connect(stackView.enterCategory)
            currentItem.searchCategory.connect(placeSearchModel.searchForCategory)
        }

        function showSuggestions()
        {
            if (currentItem.objectName != "suggestionView") {
                stackView.pop(page)
                push({ item: Qt.resolvedUrl("views/SuggestionView.qml") ,
                         properties: { "suggestionModel": suggestionModel }
                     })
                currentItem.objectName = "suggestionView"
                currentItem.suggestionSelected.connect(searchBar.showSearch)
                currentItem.suggestionSelected.connect(placeSearchModel.searchForText)
            }
        }

        function showPlaces()
        {
            if (currentItem.objectName != "searchResultView") {
                stackView.pop({tem:page,immediate: true})
                push({ item: Qt.resolvedUrl("views/SearchResultView.qml") ,
                         properties: { "placeSearchModel": placeSearchModel }
                     })
                currentItem.showPlaceDetails.connect(showPlaceDatails)
                currentItem.showMap.connect(searchBar.showMap)
                currentItem.objectName = "searchResultView"
            }
        }

        function showPlaceDatails(place, distance)
        {
            push({ item: Qt.resolvedUrl("forms/PlaceDetails.qml") ,
                     properties: { "place": place,
                         "distanceToPlace": distance }
                 })
            currentItem.searchForSimilar.connect(searchForSimilar)
            currentItem.showReviews.connect(showReviews)
            currentItem.showEditorials.connect(showEditorials)
            currentItem.showImages.connect(showImages)
        }

        function showEditorials(place)
        {
            push({ item: Qt.resolvedUrl("views/EditorialView.qml") ,
                     properties: { "place": place }
                 })
            currentItem.showEditorial.connect(showEditorial)
        }

        function showReviews(place)
        {
            push({ item: Qt.resolvedUrl("views/ReviewView.qml") ,
                     properties: { "place": place }
                 })
            currentItem.showReview.connect(showReview)
        }

        function showImages(place)
        {
            push({ item: Qt.resolvedUrl("views/ImageView.qml") ,
                     properties: { "place": place }
                 })
        }

        function showEditorial(editorial)
        {
            push({ item: Qt.resolvedUrl("views/EditorialPage.qml") ,
                     properties: { "editorial": editorial }
                 })
        }

        function showReview(review)
        {
            push({ item: Qt.resolvedUrl("views/ReviewPage.qml") ,
                     properties: { "review": review }
                 })
        }

        function changeSearchCenter(coordinate)
        {
            stackView.pop(page)
            map.center = coordinate;
            if (searchRegionItem) {
                map.removeMapItem(searchRegionItem);
                searchRegionItem.destroy();
            }
        }

        function changeSearchBoundingBox(coordinate,widthDeg,heightDeg)
        {
            stackView.pop(page)
            map.center = coordinate
            searchRegion = QtPositioning.rectangle(map.center, widthDeg, heightDeg)
            if (searchRegionItem) {
                map.removeMapItem(searchRegionItem);
                searchRegionItem.destroy();
            }
            searchRegionItem = Qt.createQmlObject('import QtLocation 5.3; MapRectangle { color: "#46a2da"; border.color: "#190a33"; border.width: 2; opacity: 0.25 }', page, "MapRectangle");
            searchRegionItem.topLeft = searchRegion.topLeft;
            searchRegionItem.bottomRight = searchRegion.bottomRight;
            map.addMapItem(searchRegionItem);
        }

        function changeSearchBoundingCircle(coordinate,radius)
        {
            stackView.pop(page)
            map.center = coordinate;
            searchRegion = QtPositioning.circle(coordinate, radius)

            if (searchRegionItem) {
                map.removeMapItem(searchRegionItem);
                searchRegionItem.destroy();
            }
            searchRegionItem = Qt.createQmlObject('import QtLocation 5.3; MapCircle { color: "#46a2da"; border.color: "#190a33"; border.width: 2; opacity: 0.25 }', page, "MapRectangle");
            searchRegionItem.center = searchRegion.center;
            searchRegionItem.radius = searchRegion.radius;
            map.addMapItem(searchRegionItem);
        }

        function changeSearchSettings(orderByDistance, orderByName, locales)
        {
            stackView.pop(page)
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
            map.plugin.locales = locales.split(Qt.locale().groupSeparator);
        }

        //! [PlaceRecommendationModel search]
        function searchForSimilar(place) {
            stackView.pop(page)
            searchBar.showSearch(place.name)
            placeSearchModel.searchForRecommendations(place.placeId);
        }
        //! [PlaceRecommendationModel search]

        anchors.fill: parent
        focus: true
        initialItem:  Item {
            id: page

            //! [PlaceSearchModel model]
            PlaceSearchModel {
                id: placeSearchModel
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
                        if (count > 0)
                            stackView.showPlaces()
                        else
                            stackView.showMessage(qsTr("Search Place Error"),qsTr("Place not found !"))
                        break;
                    case PlaceSearchModel.Error:
                        stackView.showMessage(qsTr("Search Place Error"),errorString())
                        break;
                    }
                }
            }
            //! [PlaceSearchModel model]

            //! [PlaceSearchSuggestionModel model]
            PlaceSearchSuggestionModel {
                id: suggestionModel
                searchArea: searchRegion

                onStatusChanged: {
                    if (status == PlaceSearchSuggestionModel.Ready)
                        stackView.showSuggestions()
                }
            }
            //! [PlaceSearchSuggestionModel model]

            //! [CategoryModel model]
            CategoryModel {
                id: categoryModel
                hierarchical: true
            }
            //! [CategoryModel model]

            Component {
                id: mapComponent

                MapComponent {
                    width: page.width
                    height: page.height

                    onErrorChanged: {
                        if (map.error != Map.NoError) {
                            var title = qsTr("ProviderError");
                            var message =  map.errorString + "<br/><br/><b>" + qsTr("Try to select other provider") + "</b>";
                            if (map.error == Map.MissingRequiredParameterError)
                                message += "<br/>" + qsTr("or see") + " \'mapviewer --help\' "
                                        + qsTr("how to pass plugin parameters.");
                            stackView.showMessage(title,message);
                        }
                    }

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
                                    onClicked: stackView.showPlaceDatails(model.place,model.distance)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        color: "white"
        opacity: busyIndicator.running ? 0.8 : 0
        anchors.fill: parent
        Behavior on opacity { NumberAnimation{} }
    }
    BusyIndicator {
        id: busyIndicator
        anchors.centerIn: parent
        running: placeSearchModel.status == PlaceSearchModel.Loading ||
                 categoryModel.status === CategoryModel.Loading
    }
}
