/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the documentation of the QtEnginio library.
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
import Enginio 1.0
import "../../examples/qml/config.js" as Config

Rectangle {
    width: 400
    height: 400

    EnginioClient {
        id: client
        backendId: Config.id
        onFinished: console.log("Engino request finished." + reply.data)
        onError: console.log("Enginio error " + reply.errorCode + ": " + reply.errorString)
    }

    //! [model]
    EnginioModel {
        id: enginioModel
        client: client
        query: { "objectType": "objects.city" }
    }
    //! [model]

    //! [view]
    ListView {
        anchors.fill: parent
        model: enginioModel
        delegate: Text {
            text: name + ": " + population
        }
    }
    //! [view]

    Rectangle {
        color: "lightsteelblue"
        width: parent.width
        height: 30
        anchors.bottom: parent.bottom
        Text {
            anchors.centerIn: parent
            text: "Add Berlin"
        }
        MouseArea {
            anchors.fill: parent
            onClicked: addCity()
        }
    }

    //! [append]
    function addCity() {
        var berlin = {
            "objectType": "objects.city",
            "name": "Berlin",
            "population": 3300000
        }
        enginioModel.append(berlin)
    }
    //! [append]

}
