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

ToolBar {

    property bool busyIndicatorRunning : false
    property bool searchBarVisbile: true

    signal doSearch(string searchText)
    signal searchTextChanged(string searchText)
    signal showCategories()
    signal goBack()
    signal showMap()

    onSearchBarVisbileChanged: {
        searchBar.opacity = searchBarVisbile ? 1 : 0
        backBar.opacity = searchBarVisbile ? 0 : 1
    }

    function showSearch(text) {
        if (text != null) {
            searchText.ignoreTextChange = true
            searchText.text = text
            searchText.ignoreTextChange = false
        }
    }

    RowLayout {
        id: searchBar
        width: parent.width
        height: parent.height
        Behavior on opacity { NumberAnimation{} }
        visible: opacity ? true : false
        TextField {
            id: searchText
            Behavior on opacity { NumberAnimation{} }
            visible: opacity ? true : false
            property bool ignoreTextChange: false
            placeholderText: qsTr("Type place...")
            Layout.fillWidth: true
            onTextChanged: {
                if (!ignoreTextChange)
                    searchTextChanged(text)
            }
            onAccepted: doSearch(searchText.text)
        }
        ToolButton {
            id: searchButton
            iconSource:  "../../resources/search.png"
            onClicked: doSearch(searchText.text)
        }
        ToolButton {
            id: categoryButton
            iconSource:  "../../resources/categories.png"
            onClicked: showCategories()
        }
    }

    RowLayout {
        id: backBar
        width: parent.width
        height: parent.height
        opacity: 0
        Behavior on opacity { NumberAnimation{} }
        visible: opacity ? true : false
        ToolButton {
            id: backButton
            iconSource:  "../../resources/left.png"
            onClicked: goBack()
        }
        ToolButton {
            id: mapButton
            iconSource:  "../../resources/search.png"
            onClicked: showMap()
        }
        Item {
             Layout.fillWidth: true
        }
    }
}

