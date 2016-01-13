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

Dialog {
    id: dialog

    property alias isFavoritesEnabled: enableFavoritesButton.selected
    property alias orderByDistance: orderByDistanceButton.selected
    property alias orderByName: orderByNameButton.selected
    property alias locales: localesInput.text
    property int listItemHeight: 21

    title: "Options"

    item: Column {
        id: options
        width: parent.width
        spacing: gap

        TextWithLabel {
            id: localesInput

            width: parent.width - gap
            height: listItemHeight
            label: "Locale(s)"
            enabled: true
            visible: placesPlugin.name != "" ? placesPlugin.supportsPlaces(Plugin.LocalizedPlacesFeature) : false;
        }

        Optionbutton {
            id: enableFavoritesButton

            function resetVisibility() {
                //jsondb plug-in is no more but saving of places may come back
                /*if (placesPlugin.name !== "places_jsondb") {
                    var pluginNames = placesPlugin.availableServiceProviders;
                    for (var i = 0; i < pluginNames.length; ++i) {
                        if (pluginNames[i] === "places_jsondb") {
                            enableFavoritesButton.visible = true;
                            return;
                        }
                    }
                }*/
                enableFavoritesButton.visible = false;
            }

            width: parent.width
            text: "Enable favorites"
            toggle:  true
            visible: false

            Component.onCompleted: {
                resetVisibility();
                placesPlugin.nameChanged.connect(resetVisibility);
            }
        }

        Optionbutton {
            id: orderByDistanceButton
            width: parent.width
            text: "Order by distance"
            toggle: true
            visible: true
            onClicked:
                if (selected)
                    orderByNameButton.selected = false;
        }
        Optionbutton {
            id: orderByNameButton
            width: parent.width
            text: "Order by name"
            toggle: true
            visible: true
            onClicked:
                if (selected)
                    orderByDistanceButton.selected = false;
        }
    }
}
