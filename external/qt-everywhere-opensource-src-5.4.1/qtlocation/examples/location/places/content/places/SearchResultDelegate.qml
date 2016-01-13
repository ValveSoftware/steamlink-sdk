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
import "PlacesUtils.js" as PlacesUtils

Item {
    id: root

    signal displayPlaceDetails(variant data)
    signal searchFor(string query)

    width: parent.width
    height: childrenRect.height + 20

    //! [PlaceSearchModel place delegate]
    Component {
        id: placeComponent

        Item {
            id: placeRoot

            height: childrenRect.height
            width: parent.width

            Rectangle {
                anchors.fill: parent
                color: "#dbffde"
                visible: model.sponsored !== undefined ? model.sponsored : false

                Text {
                    text: qsTr("Sponsored result")
                    horizontalAlignment: Text.AlignRight
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    font.pixelSize: 8
                    visible: model.sponsored !== undefined ? model.sponsored : false
                }
            }

            Row {
                Image {
                    source: place.favorite ? "../../resources/star.png" : place.icon.url()
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        id: placeName
                        text: place.favorite ? place.favorite.name : place.name
                    }

                    Text {
                        id: distanceText
                        font.italic: true
                        text: PlacesUtils.prettyDistance(distance)
                    }
                }
            }

            MouseArea {
                anchors.fill: parent

                onPressed: placeRoot.state = "Pressed"
                onReleased: placeRoot.state = ""
                onCanceled: placeRoot.state = ""

                onClicked: {
                    if (model.type === undefined || type === PlaceSearchModel.PlaceResult) {
                        if (!place.detailsFetched)
                            place.getDetails();

                        root.displayPlaceDetails({
                                                 distance: model.distance,
                                                 place: model.place,
                    });
                    }
                }
            }

            states: [
                State {
                    name: ""
                },
                State {
                    name: "Pressed"
                    PropertyChanges { target: placeName; color: "#1C94FC"}
                    PropertyChanges { target: distanceText; color: "#1C94FC"}
                }
            ]
        }
    }
    //! [PlaceSearchModel place delegate]

    Component {
        id: proposedSearchComponent

        Item {
            id: proposedSearchRoot

            height: childrenRect.height
            width: parent.width

            Row {
                Image {
                    source: icon.url()
                }

                Text {
                    id: proposedSearchTitle
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Search for " + title
                }
            }

            MouseArea {
                anchors.fill: parent

                onPressed: proposedSearchRoot.state = "Pressed"
                onReleased: proposedSearchRoot.state = ""
                onCanceled: proposedSearchRoot.state = ""

                onClicked: root.ListView.view.model.updateWith(index);
            }

            states: [
                State {
                    name: ""
                },
                State {
                    name: "Pressed"
                    PropertyChanges { target: proposedSearchTitle; color: "#1C94FC"}
                }
            ]
        }
    }

    Loader {
        anchors.left: parent.left
        anchors.right: parent.right

        sourceComponent: {
            switch (model.type) {
            case PlaceSearchModel.PlaceResult:
                return placeComponent;
            case PlaceSearchModel.ProposedSearchResult:
                return proposedSearchComponent;
            default:
                //do nothing, don't assign component if result type not recognized
            }
        }
    }
}
