/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

import QtQml 2.0
import QtQuick 2.0
import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.0
import QtQuick.Layouts 1.0
import QtQuick.Window 2.0
import QtWebEngine 1.4

ApplicationWindow {
    id: appWindow
    title: qsTr("Recipe Browser")
    visible: true

    property int shorterDesktop: 768
    property int longerDesktop: 1024
    property int shorterMin: 360
    property int longerMin: 480
    property bool isPortrait: Screen.primaryOrientation === Qt.PortraitOrientation
    width: {
        if (isEmbedded)
            return Screen.width
        var potentialWidth = shorterDesktop
        if (!isPortrait)
            potentialWidth = longerDesktop
        return potentialWidth > Screen.width ? Screen.width : potentialWidth
    }
    height: {
        if (isEmbedded)
            return Screen.height
        var potentialHeight = longerDesktop
        if (!isPortrait)
            potentialHeight = shorterDesktop
        return potentialHeight > Screen.height ? Screen.height : potentialHeight
    }
    minimumWidth: isPortrait ? shorterMin : longerMin
    minimumHeight: isPortrait ? longerMin : shorterMin

    RowLayout {
        id: container
        anchors.fill: parent
        spacing: 0

        RecipeList {
            id: recipeList
            Layout.minimumWidth: 124
            Layout.preferredWidth: parent.width / 3
            Layout.maximumWidth: 300
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: true
            KeyNavigation.tab: webView
            onRecipeSelected: webView.showRecipe(url)
        }

        WebEngineView {
            id: webView
            Layout.preferredWidth: 2 * parent.width / 3
            Layout.fillWidth: true
            Layout.fillHeight: true
            KeyNavigation.tab: recipeList
            KeyNavigation.priority: KeyNavigation.BeforeItem
            // Make sure focus is not taken by the web view, so user can continue navigating
            // recipes with the keyboard.
            settings.focusOnNavigationEnabled: false

            onContextMenuRequested: function(request) {
                request.accepted = true
            }

            property bool firstLoadComplete: false
            onLoadingChanged: {
                if (loadRequest.status === WebEngineView.LoadSucceededStatus
                    && !firstLoadComplete) {
                    // Debounce the showing of the web content, so images are more likely
                    // to have loaded completely.
                    showTimer.start()
                }
            }

            Timer {
                id: showTimer
                interval: 500
                repeat: false
                onTriggered: {
                    webView.show(true)
                    webView.firstLoadComplete = true
                    recipeList.showHelp()
                }
            }

            Rectangle {
                id: webViewPlaceholder
                anchors.fill: parent
                z: 1
                color: "white"

                BusyIndicator {
                    id: busy
                    anchors.centerIn: parent
                }
            }

            function showRecipe(url) {
                webView.url = url
            }

            function show(show) {
                if (show === true) {
                    busy.running = false
                    webViewPlaceholder.visible = false
                } else {
                    webViewPlaceholder.visible = true
                    busy.running = true
                }
            }
        }
    }
}
