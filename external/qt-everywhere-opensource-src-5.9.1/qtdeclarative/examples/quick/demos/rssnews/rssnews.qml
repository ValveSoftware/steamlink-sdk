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

import QtQuick 2.2
import QtQuick.XmlListModel 2.0
import QtQuick.Window 2.1
import "./content"

Rectangle {
    id: window

    width: 800
    height: 480

    property string currentFeed: rssFeeds.get(0).feed
    property bool loading: feedModel.status === XmlListModel.Loading
    property bool isPortrait: Screen.primaryOrientation === Qt.PortraitOrientation

    onLoadingChanged: {
        if (feedModel.status == XmlListModel.Ready)
            list.positionViewAtBeginning()
    }

    RssFeeds { id: rssFeeds }

    XmlListModel {
        id: feedModel

        source: "http://" + window.currentFeed
        query: "/rss/channel/item[child::media:content]"
        namespaceDeclarations: "declare namespace media = 'http://search.yahoo.com/mrss/';"

        XmlRole { name: "title"; query: "title/string()" }
        // Remove any links from the description
        XmlRole { name: "description"; query: "fn:replace(description/string(), '\&lt;a href=.*\/a\&gt;', '')" }
        XmlRole { name: "image"; query: "media:content/@url/string()" }
        XmlRole { name: "link"; query: "link/string()" }
        XmlRole { name: "pubDate"; query: "pubDate/string()" }
    }

    ListView {
        id: categories
        property int itemWidth: 190

        width: isPortrait ? parent.width : itemWidth
        height: isPortrait ? itemWidth : parent.height
        orientation: isPortrait ? ListView.Horizontal : ListView.Vertical
        anchors.top: parent.top
        model: rssFeeds
        delegate: CategoryDelegate { itemSize: categories.itemWidth }
        spacing: 3
    }

    ScrollBar {
        id: listScrollBar

        orientation: isPortrait ? Qt.Horizontal : Qt.Vertical
        height: isPortrait ? 8 : categories.height;
        width: isPortrait ? categories.width : 8
        scrollArea: categories;
        anchors.right: categories.right
    }

    ListView {
        id: list

        anchors.left: isPortrait ? window.left : categories.right
        anchors.right: closeButton.left
        anchors.top: isPortrait ? categories.bottom : window.top
        anchors.bottom: window.bottom
        anchors.leftMargin: 30
        anchors.rightMargin: 4
        clip: isPortrait
        model: feedModel
        footer: footerText
        delegate: NewsDelegate {}
    }

    ScrollBar {
        scrollArea: list
        width: 8
        anchors.right: window.right
        anchors.top: isPortrait ? categories.bottom : window.top
        anchors.bottom: window.bottom
    }

    Component {
        id: footerText

        Rectangle {
            width: parent.width
            height: closeButton.height
            color: "lightgray"

            Text {
                text: "RSS Feed from Yahoo News"
                anchors.centerIn: parent
                font.pixelSize: 14
            }
        }
    }

    Image {
        id: closeButton
        source: "content/images/btn_close.png"
        scale: 0.8
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 4
        opacity: (isPortrait && categories.moving) ? 0.2 : 1.0
        Behavior on opacity {
            NumberAnimation { duration: 300; easing.type: Easing.OutSine }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                Qt.quit()
            }
        }
    }
}
