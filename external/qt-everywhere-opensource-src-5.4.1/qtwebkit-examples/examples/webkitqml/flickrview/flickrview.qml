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
import QtWebKit 3.0
import QtQuick.XmlListModel 2.0
import "qrc:/shared" as Shared

Rectangle {
    id: container
    width: 1024
    height: 768

    property string initialUrl: "https://www.flickr.com/explore/interesting/7days/?"

    Rectangle {
        id: thumbnailContainer
        color: "black"

        anchors.bottom: container.bottom
        width: container.width
        height: 100

        gradient: Gradient {
            GradientStop { position: 0.0; color: "gray" }
            GradientStop { position: 0.33; color: "black" }
        }

        Text {
            id: info
            color: "white"
            anchors.horizontalCenter: thumbnailContainer.horizontalCenter
            text: webView.title
        }

        ListView {
            id: listView
            orientation: "Horizontal"
            anchors {
                topMargin: 20
                fill: parent
            }

            model: model
            delegate: Component {
                Image {
                    source: thumbnail
                    MouseArea {
                        anchors.fill: parent
                        onClicked: webView.url = link + "/lightbox"
                    }
                }
            }

            focus: true
            spacing: 10
            leftMargin: 10
            rightMargin: 35
            visible: model.status == XmlListModel.Ready
        }

        Rectangle {
            id: updateInfo

            property real distance: -(listView.contentWidth - listView.contentX - thumbnailContainer.width)
            property real threshold: Math.max(2.5 * listView.height, thumbnailContainer.width - listView.contentWidth + 2 * listView.height)
            property bool triggerUpdate: false

            opacity: 0.8
            x: thumbnailContainer.width - distance
            width: listView.height
            color: "transparent"

            anchors {
                top: thumbnailContainer.top
                bottom: thumbnailContainer.bottom
            }

            Timer {
                interval: 200; running: updateInfo.state == "update"; repeat: false
                onTriggered: { model.reload(); updateInfo.triggerUpdate = false; }
            }

            states: [
                State {
                    name: "pull"
                    when: updateInfo.distance <= updateInfo.threshold && listView.dragging
                    PropertyChanges { target: message; text: "Pull\nto\nupdate" }
                },

                State {
                    name: "release"
                    when: updateInfo.distance > updateInfo.threshold && listView.dragging
                    PropertyChanges { target: message; text: "Release\nto\nupdate" }
                },

                State {
                    name: "update"
                    when: updateInfo.triggerUpdate && listView.atXEnd && !listView.dragging
                    PropertyChanges { target: message; text: "Updating" }
                }
            ]

            onStateChanged: {
                if (state == "release")
                    triggerUpdate = true
                if (state == "pull")
                    triggerUpdate = false
            }

            Rectangle {
                id: icon
                width: 30
                color: "transparent"
                anchors {
                    topMargin: 10
                    top: parent.top
                    bottom: parent.bottom
                    left: parent.left
                }
                Image {
                    source: "qrc:/shared/images/arrow.png"
                    width: 30
                    height: 30
                    visible: updateInfo.state != "update"
                    rotation: updateInfo.state == "release" ? 180 : 0
                    Behavior on rotation { NumberAnimation { duration: 100} }
                    SequentialAnimation on x {
                        running: listView.atXEnd && !listView.dragging
                        loops: Animation.Infinite
                        PropertyAnimation { to: 5; duration: 250 }
                        PropertyAnimation { to: 0; duration: 250 }
                    }
                    anchors {
                        verticalCenter: parent.verticalCenter
                    }
                }
            }

            Text {
                id: message
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.family: "Monospace"
                color: "white"
                anchors {
                    top: parent.top
                    bottom: parent.bottom
                    right: parent.right
                    left: icon.right
                }
            }
        }

        Shared.LoadIndicator {
            anchors.fill: parent
            color: "black"
            running: !listView.visible && model.status != XmlListModel.Error
        }
    }

    Rectangle {
        id: content
        width: container.width
        color: "black"
        anchors {
            top: container.top
            bottom: thumbnailContainer.top
        }

        WebView {
            id: webView
            anchors.fill: parent
            opacity: 0

            url: container.initialUrl

            Behavior on opacity {
                NumberAnimation { duration: 200 }
            }

            onLoadingChanged: {
                switch (loadRequest.status)
                {
                case WebView.LoadSucceededStatus:
                    opacity = 1
                    break
                default:
                    opacity = 0
                    break
                }
            }

            onNavigationRequested: {
                switch (request.navigationType)
                {
                case WebView.LinkClickedNavigation:
                case WebView.FormSubmittedNavigation:
                case WebView.BackForwardNavigation:
                case WebView.ReloadNavigation:
                case WebView.FormResubmittedNavigation:
                case WebView.OtherNavigation:
                    if (/^(https|http):\/\/(www\.flickr\.com|login\.yahoo\.com)/.test(request.url)) {
                        request.action = WebView.AcceptRequest
                        return
                    }
                }
                // Disallow navigating outside of flickr.com
                request.action = WebView.IgnoreRequest
            }
        }

        Shared.LoadIndicator {
            anchors.fill: parent
            imageSource: "qrc:/images/flickr.png"
            running: webView.loading
        }
    }

    XmlListModel {
        id: model
        namespaceDeclarations: "declare namespace media=\"http://search.yahoo.com/mrss/\";"
        source: "http://api.flickr.com/services/feeds/photos_public.gne?format=rss2"
        query: "/rss/channel/item"
        XmlRole { name: "thumbnail"; query: "media:thumbnail/@url/string()" }
        XmlRole { name: "thumbnailHeight"; query: "media:thumbnail/@height/number()" }
        XmlRole { name: "content"; query: "media:content/@url/string()" }
        XmlRole { name: "link"; query: "link/string()" }
    }

}
