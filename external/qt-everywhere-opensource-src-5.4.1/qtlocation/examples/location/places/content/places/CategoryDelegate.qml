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

Item {
    id: root

    property bool showSave: true
    property bool showRemove: true
    property bool showChildren: true

    signal clicked
    signal arrowClicked
    signal crossClicked
    signal editClicked

    width: parent.width
    height: textItem.height

    Item {
        id: textItem
        anchors.left: parent.left
        anchors.right: arrow.left
        anchors.verticalCenter: parent.verticalCenter

        height: Math.max(icon.height, name.height)

        Image {
            id: icon

            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            source: category.icon.url()
        }

        //! [CategoryModel delegate text]
        Text {
            id: name

            anchors.left: icon.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right

            verticalAlignment: Text.AlignVCenter

            text: category.name
            elide: Text.ElideRight
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.clicked()
        }
        //! [CategoryModel delegate text]
    }

    //! [CategoryModel delegate icon]
    IconButton {
        id: edit

        anchors.right: cross.left
        anchors.verticalCenter: parent.verticalCenter

        visible: (placesPlugin.name != "" ? placesPlugin.supportsPlaces(Plugin.SaveCategoryFeature) : false)
                 && showSave

        source: "../../resources/pencil.png"
        hoveredSource: "../../resources/pencil_hovered.png"
        pressedSource: "../../resources/pencil_pressed.png"

        onClicked: root.editClicked()
    }

    IconButton {
        id: cross

        anchors.right: arrow.left
        anchors.verticalCenter: parent.verticalCenter
        visible: (placesPlugin.name != "" ? placesPlugin.supportsPlaces(Plugin.RemoveCategoryFeature) : false)
                 && showRemove

        source: "../../resources/cross.png"
        hoveredSource: "../../resources/cross_hovered.png"
        pressedSource: "../../resources/cross_pressed.png"

        onClicked: root.crossClicked()
    }

    IconButton {
        id: arrow

        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        visible: model.hasModelChildren && showChildren

        source: "../../resources/right.png"
        pressedSource: "../../resources/right_pressed.png"

        onClicked: root.arrowClicked()
    }
    //! [CategoryModel delegate icon]
}
