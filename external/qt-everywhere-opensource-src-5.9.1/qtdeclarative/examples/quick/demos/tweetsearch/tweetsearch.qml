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

import QtQuick 2.0
import "content"
import "content/tweetsearch.js" as Helper

Rectangle {
    id: main
    width: 320
    height: 480
    color: "#d6d6d6"

    property int inAnimDur: 250
    property int counter: 0
    property alias isLoading: tweetsModel.isLoading
    property var idx
    property var ids

    Component.onCompleted: ids = new Array()

    function idInModel(id)
    {
        for (var j = 0; j < ids.length; j++)
            if (ids[j] === id)
                return 1
        return 0
    }

    TweetsModel {
        id: tweetsModel
        onIsLoaded: {
            console.debug("Reload")
            idx = new Array()
            for (var i = 0; i < tweetsModel.model.count; i++) {
                var id = tweetsModel.model.get(i).id
                if (!idInModel(id))
                    idx.push(i)
            }
            console.debug(idx.length + " new tweets")
            main.counter = idx.length
        }
    }

    Timer {
        id: timer
        interval: 500; running: main.counter; repeat: true
        onTriggered: {
            main.counter--;
            var id = tweetsModel.model.get(idx[main.counter]).id
            var item = tweetsModel.model.get(main.counter)
            mainListView.add( { "statusText": Helper.insertLinks(item.text, item.entities),
                                "twitterName": item.user.screen_name,
                                "name" : item.user.name,
                                "userImage": item.user.profile_image_url,
                                "source": item.source,
                                "id": id,
                                 "uri": Helper.insertLinks(item.user.url, item.user.entities),
                                "published": item.created_at } );
            ids.push(id)
        }
    }

    ListView {
        id: mainListView
        anchors.fill: parent
        delegate: TweetDelegate { }
        model: ListModel { id: finalModel }

        add: Transition {
            NumberAnimation { property: "hm"; from: 0; to: 1.0; duration: 300; easing.type: Easing.OutQuad }
            PropertyAction { property: "appear"; value: 250 }
        }

        onDragEnded: if (header.refresh) { tweetsModel.reload() }

        ListHeader {
            id: header
            y: -mainListView.contentY - height
        }

        footer: ListFooter { }

        function clear() {
            ids = new Array()
            model.clear()
        }

        function add(obj) {
            model.insert(0, obj)
        }

        signal autoSearch(string type, string str) // To communicate with Footer instance
    }
}
