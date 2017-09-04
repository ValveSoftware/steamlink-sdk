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
import QtLocation 5.6

MenuBar {
    property variant  providerMenu: providerMenu
    property variant  mapTypeMenu: mapTypeMenu
    property variant  toolsMenu: toolsMenu
    property alias isFollowMe: toolsMenu.isFollowMe
    property alias isMiniMap: toolsMenu.isMiniMap

    signal selectProvider(string providerName)
    signal selectMapType(variant mapType)
    signal selectTool(string tool);
    signal toggleMapState(string state)

    Menu {
        id: providerMenu
        title: qsTr("Provider")

        function createMenu(plugins)
        {
            clear()
            for (var i = 0; i < plugins.length; i++) {
                createProviderMenuItem(plugins[i]);
            }
        }

        function createProviderMenuItem(provider)
        {
            var item = addItem(provider);
            item.checkable = true;
            item.triggered.connect(function(){selectProvider(provider)})
        }
    }

    Menu {
        id: mapTypeMenu
        title: qsTr("MapType")

        function createMenu(map)
        {
            clear()
            for (var i = 0; i<map.supportedMapTypes.length; i++) {
                createMapTypeMenuItem(map.supportedMapTypes[i]).checked =
                        (map.activeMapType === map.supportedMapTypes[i]);
            }
        }

        function createMapTypeMenuItem(mapType)
        {
            var item = addItem(mapType.name);
            item.checkable = true;
            item.triggered.connect(function(){selectMapType(mapType)})
            return item;
        }
    }

    Menu {
        id: toolsMenu
        property bool isFollowMe: false;
        property bool isMiniMap: false;
        title: qsTr("Tools")

        function createMenu(map)
        {
            clear()
            if (map.plugin.supportsGeocoding(Plugin.ReverseGeocodingFeature)) {
                addItem(qsTr("Reverse geocode")).triggered.connect(function(){selectTool("RevGeocode")})
            }
            if (map.plugin.supportsGeocoding()) {
                addItem(qsTr("Geocode")).triggered.connect(function(){selectTool("Geocode")})
            }
            if (map.plugin.supportsRouting()) {
                addItem(qsTr("Route with coordinates")).triggered.connect(function(){selectTool("CoordinateRoute")})
                addItem(qsTr("Route with address")).triggered.connect(function(){selectTool("AddressRoute")})
            }

            var item = addItem("")
            item.text = Qt.binding(function() { return isMiniMap ? qsTr("Hide minimap") : qsTr("Minimap") })
            item.triggered.connect(function() {toggleMapState("MiniMap")})

            item = addItem("")
            item.text = Qt.binding(function() { return isFollowMe ? qsTr("Stop following") : qsTr("Follow me")})
            item.triggered.connect(function() {toggleMapState("FollowMe")})

            addItem(qsTr("Language")).triggered.connect(function(){selectTool("Language")})
            addItem(qsTr("Prefetch Map Data")).triggered.connect(function(){selectTool("Prefetch")})
            addItem(qsTr("Clear Map Data")).triggered.connect(function(){selectTool("Clear")})
        }
    }
}
