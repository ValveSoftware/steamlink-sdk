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

Menu {
    property int currentMarker
    property int markersCount
    signal itemClicked(string item)

    function update() {
        clear()
        addItem(qsTr("Delete")).triggered.connect(function(){itemClicked("deleteMarker")})
        addItem(qsTr("Coordinates")).triggered.connect(function(){itemClicked("getMarkerCoordinate")})
        addItem(qsTr("Move to")).triggered.connect(function(){itemClicked("moveMarkerTo")})
        if (currentMarker == markersCount-2){
            addItem(qsTr("Route to next point")).triggered.connect(function(){itemClicked("routeToNextPoint")});
            addItem(qsTr("Distance to next point")).triggered.connect(function(){itemClicked("distanceToNextPoint")});
        }
        if (currentMarker < markersCount-2){
            addItem(qsTr("Route to next points")).triggered.connect(function(){itemClicked("routeToNextPoints")});
            addItem(qsTr("Distance to next point")).triggered.connect(function(){itemClicked("distanceToNextPoint")});
        }

        var menu = addMenu(qsTr("Draw..."))
        menu.addItem(qsTr("Image")).triggered.connect(function(){itemClicked("drawImage")})

        if (currentMarker <= markersCount-2){
            menu.addItem(qsTr("Rectangle")).triggered.connect(function(){itemClicked("drawRectangle")})
            menu.addItem(qsTr("Circle")).triggered.connect(function(){itemClicked("drawCircle")})
            menu.addItem(qsTr("Polyline")).triggered.connect(function(){itemClicked("drawPolyline")})
        }

        if (currentMarker < markersCount-2){
            menu.addItem(qsTr("Polygon")).triggered.connect(function(){itemClicked("drawPolygonMenu")})
        }
    }
}
