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
import QtLocation.examples 5.0

Rectangle {
    id: searchRectangle

    property bool suggestionsEnabled: true
    property int expandedHeight: childrenRect.height
    readonly property int baseHeight: searchBox.height + 20

    color: "#ECECEC"

    height: baseHeight
    Behavior on height {
        NumberAnimation { duration: 250 }
    }

    clip: true

    TextWithLabel {
        id: searchBox
        label: "Search"
        text: "sushi"

        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.right: row.left
        anchors.rightMargin: 10
        anchors.top: parent.top
        anchors.topMargin: 10

        busy: placeSearchModel.status === PlaceSearchModel.Loading

        //! [PlaceSearchSuggestionModel search text changed]
        onTextChanged: {
            if (searchRectangle.suggestionsEnabled) {
                if (text.length >= 3) {
                    if (suggestionModel != null) {
                        suggestionModel.searchTerm = text;
                        suggestionModel.update();
                    }
                } else {
                    searchRectangle.state = "";
                }
            }
        }
        //! [PlaceSearchSuggestionModel search text changed]
    }

    Row {
        id: row

        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.top: parent.top
        anchors.topMargin: 10
        spacing: 10

        IconButton {
            id: searchButton

            anchors.verticalCenter:  parent.verticalCenter

            source: "../../resources/search.png"
            pressedSource: "../../resources/search_pressed.png"

            onClicked: {
                placeSearchModel.searchForText(searchBox.text);
                searchRectangle.state = "";
            }
        }

        IconButton {
            id: categoryButton

            source: "../../resources/categories.png"
            pressedSource: "../../resources/categories_pressed.png"

            onClicked: {
                if (searchRectangle.state !== "CategoriesShown")
                    searchRectangle.state = "CategoriesShown";
                else if (suggestionView.count > 0)
                    searchRectangle.state = "SuggestionsShown";
                else
                    searchRectangle.state = "";
            }
        }
    }

    CategoryView {
        id: categoryView

        anchors.top: row.bottom
        height: expandedHeight - y
        visible: false
        spacing: 5

        onCategoryClicked: {
            placeSearchModel.searchForCategory(category);
            searchRectangle.state = "";
        }

        onEditClicked: {
            editCategoryDialog.category = category;
            page.state = "EditCategory";
            searchRectangle.state = "";
        }
    }

    BusyIndicator {
        id: busy

        visible: false

        anchors.centerIn: parent
    }

    Text {
        id: noCategories

        anchors.centerIn: parent
        text: qsTr("No categories")
        visible: false
    }

    //! [PlaceSearchSuggestionModel view 1]
    ListView {
        id: suggestionView
    //! [PlaceSearchSuggestionModel view 1]

        anchors.top: row.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.right: parent.right
        anchors.rightMargin: 10
        height: 450
        visible: false

        clip: true
        snapMode: ListView.SnapToItem

    //! [PlaceSearchSuggestionModel view 2]
        model: suggestionModel
        delegate: Text {
            text: suggestion

            width: parent.width

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    suggestionsEnabled = false;
                    searchBox.text = suggestion;
                    suggestionsEnabled = true;
                    placeSearchModel.searchForText(suggestion);
                    searchRectangle.state = "";
                }
            }
        }
    }
    //! [PlaceSearchSuggestionModel view 2]

    //! [PlaceSearchSuggestionModel model]
    PlaceSearchSuggestionModel {
        id: suggestionModel
        plugin: placesPlugin
        searchArea: placeSearchModel.searchArea

        onStatusChanged: {
            if (status == PlaceSearchSuggestionModel.Ready)
                searchRectangle.state = "SuggestionsShown";
        }
    }
    //! [PlaceSearchSuggestionModel model]

    states: [
        State {
            name: "CategoriesShown"
            PropertyChanges {
                target: searchRectangle
                height: expandedHeight
            }
            PropertyChanges {
                target: busy
                visible: categoryModel.status === CategoryModel.Loading
            }
            PropertyChanges {
                target: noCategories
                visible: categoryView.count == 0 && !busy.visible
            }
            PropertyChanges {
                target: categoryView
                visible: true && !busy.visible
            }
        },
        State {
            name: "SuggestionsShown"
            PropertyChanges {
                target: searchRectangle
                height: childrenRect.height + 20
            }
            PropertyChanges {
                target: suggestionView
                visible: true
            }
        }
    ]
}
