/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

import QtQuick 2.1
import QtWebEngine 1.0
import QtWebEngine.experimental 1.0

import QtQuick.Controls 1.0
import QtQuick.Controls.Styles 1.0
import QtQuick.Layouts 1.0
import QtQuick.Window 2.1
import QtQuick.Controls.Private 1.0
import Qt.labs.settings 1.0


ApplicationWindow {
    id: browserWindow
    function load(url) { currentWebView.url = url }
    property Item currentWebView: tabs.currentIndex < tabs.count ? tabs.getTab(tabs.currentIndex).item.webView : null

    property bool isFullScreen: visibility == Window.FullScreen
    onIsFullScreenChanged: {
        // This is for the case where the system forces us to leave fullscreen.
        if (currentWebView)
            currentWebView.state = isFullScreen ? "FullScreen" : ""
    }

    height: 600
    width: 800
    visible: true
    title: currentWebView && currentWebView.title

    Settings {
        property alias autoLoadImages: loadImages.checked;
        property alias javaScriptEnabled: javaScriptEnabled.checked;
        property alias errorPageEnabled: errorPageEnabled.checked;
    }

    // Make sure the Qt.WindowFullscreenButtonHint is set on Mac.
    Component.onCompleted: flags = flags | Qt.WindowFullscreenButtonHint

    // Create a styleItem to determine the platform.
    // When using style "mac", ToolButtons are not supposed to accept focus.
    StyleItem { id: styleItem }
    property bool platformIsMac: styleItem.style == "mac"


    Action {
        id: focus
        shortcut: "Ctrl+L"
        onTriggered: {
            addressBar.forceActiveFocus();
            addressBar.selectAll();
        }
    }
    Action {
        shortcut: "Ctrl+R"
        onTriggered: {
            if (currentWebView)
                currentWebView.reload()
        }
    }
    Action {
        shortcut: "Ctrl+T"
        onTriggered: {
            tabs.createEmptyTab()
            addressBar.forceActiveFocus();
            addressBar.selectAll();
        }
    }
    Action {
        shortcut: "Ctrl+W"
        onTriggered: {
            if (tabs.count == 1)
                browserWindow.close()
            else
                tabs.removeTab(tabs.currentIndex)
        }
    }

    Action {
        shortcut: "Escape"
        onTriggered: {
            if (browserWindow.isFullScreen)
                browserWindow.showNormal()
        }
    }

    Menu {
        id: backHistoryMenu

        Instantiator {
            model: currentWebView && currentWebView.experimental.navigationHistory.backItems
            MenuItem {
                text: model.title
                onTriggered: currentWebView.experimental.goBackTo(index)
            }

            onObjectAdded: backHistoryMenu.insertItem(index, object)
            onObjectRemoved: backHistoryMenu.removeItem(object)
        }
    }

    Menu {
        id: forwardHistoryMenu

        Instantiator {
            model: currentWebView && currentWebView.experimental.navigationHistory.forwardItems
            MenuItem {
                text: model.title
                onTriggered: currentWebView.experimental.goForwardTo(index)
            }

            onObjectAdded: forwardHistoryMenu.insertItem(index, object)
            onObjectRemoved: forwardHistoryMenu.removeItem(object)
        }
    }

    toolBar: ToolBar {
        id: navigationBar
            RowLayout {
                anchors.fill: parent;
                ButtonWithMenu {
                    id: backButton
                    iconSource: "icons/go-previous.png"
                    enabled: currentWebView && currentWebView.canGoBack
                    activeFocusOnTab: !browserWindow.platformIsMac
                    onClicked: currentWebView.goBack()
                    longPressMenu: backHistoryMenu
                }
                ButtonWithMenu {
                    id: forwardButton
                    iconSource: "icons/go-next.png"
                    enabled: currentWebView && currentWebView.canGoForward
                    activeFocusOnTab: !browserWindow.platformIsMac
                    onClicked: currentWebView.goForward()
                    longPressMenu: forwardHistoryMenu
                }
                ToolButton {
                    id: reloadButton
                    iconSource: currentWebView && currentWebView.loading ? "icons/process-stop.png" : "icons/view-refresh.png"
                    onClicked: currentWebView && currentWebView.loading ? currentWebView.stop() : currentWebView.reload()
                    activeFocusOnTab: !browserWindow.platformIsMac
                }
                TextField {
                    id: addressBar
                    Image {
                        anchors.verticalCenter: addressBar.verticalCenter;
                        x: 5
                        z: 2
                        id: faviconImage
                        width: 16; height: 16
                        source: currentWebView && currentWebView.icon
                    }
                    style: TextFieldStyle {
                        padding {
                            left: 26;
                        }
                    }
                    focus: true
                    Layout.fillWidth: true
                    text: currentWebView && currentWebView.url
                    onAccepted: currentWebView.url = utils.fromUserInput(text)
                }
                ToolButton {
                    id: settingsMenuButton
                    menu: Menu {
                        MenuItem {
                            id: loadImages
                            text: "Autoload images"
                            checkable: true
                            checked: WebEngine.settings.autoLoadImages
                            onCheckedChanged: WebEngine.settings.autoLoadImages = checked
                        }
                        MenuItem {
                            id: javaScriptEnabled
                            text: "JavaScript On"
                            checkable: true
                            checked: WebEngine.settings.javascriptEnabled
                            onCheckedChanged: WebEngine.settings.javascriptEnabled = checked
                        }
                        MenuItem {
                            id: errorPageEnabled
                            text: "ErrorPage On"
                            checkable: true
                            checked: WebEngine.settings.errorPageEnabled
                            onCheckedChanged: WebEngine.settings.errorPageEnabled = checked
                        }
                    }
                }
            }
            ProgressBar {
                id: progressBar
                height: 3
                anchors {
                    left: parent.left
                    top: parent.bottom
                    right: parent.right
                    leftMargin: -parent.leftMargin
                    rightMargin: -parent.rightMargin
                }
                style: ProgressBarStyle {
                    background: Item {}
                }
                z: -2;
                minimumValue: 0
                maximumValue: 100
                value: (currentWebView && currentWebView.loadProgress < 100) ? currentWebView.loadProgress : 0
            }
    }

    TabView {
        id: tabs
        function createEmptyTab() {
            var tab = addTab("", tabComponent)
            // We must do this first to make sure that tab.active gets set so that tab.item gets instantiated immediately.
            tabs.currentIndex = tabs.count - 1
            tab.title = Qt.binding(function() { return tab.item.title })
            return tab
        }

        anchors.fill: parent
        Component.onCompleted: createEmptyTab()

        Component {
            id: dialogComponent
            Window {
                property Item webView: _webView
                width: 800
                height: 600
                visible: true
                WebEngineView {
                    id: _webView
                    anchors.fill: parent
                }
            }
        }

        Component {
            id: tabComponent
            Item {
                property alias webView: webEngineView
                property alias title: webEngineView.title
                Action {
                    shortcut: "Ctrl+F"
                    onTriggered: {
                        findBar.visible = !findBar.visible
                        if (findBar.visible) {
                            findTextField.forceActiveFocus()
                        }
                    }
                }
                FeaturePermissionBar {
                    id: permBar
                    view: webEngineView
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                    }
                    z: 3
                }

                WebEngineView {
                    id: webEngineView

                    anchors {
                        fill: parent
                        top: permBar.bottom
                    }

                    focus: true

                    states: [
                        State {
                            name: "FullScreen"
                            PropertyChanges {
                                target: tabs
                                frameVisible: false
                                tabsVisible: false
                            }
                            PropertyChanges {
                                target: navigationBar
                                visible: false
                            }
                        }
                    ]

                    experimental {
                        isFullScreen: webEngineView.state == "FullScreen" && browserWindow.isFullScreen
                        onFullScreenRequested: {
                            if (fullScreen) {
                                webEngineView.state = "FullScreen"
                                browserWindow.showFullScreen();
                            } else {
                                webEngineView.state = ""
                                browserWindow.showNormal();
                            }
                        }

                        onNewViewRequested: {
                            if (!request.userInitiated)
                                print("Warning: Blocked a popup window.")
                            else if (request.destination == WebEngineView.NewViewInTab) {
                                var tab = tabs.createEmptyTab()
                                request.openIn(tab.item.webView)
                            } else if (request.destination == WebEngineView.NewViewInDialog) {
                                var dialog = dialogComponent.createObject()
                                request.openIn(dialog.webView)
                            } else {
                                var component = Qt.createComponent("quickwindow.qml")
                                var window = component.createObject()
                                request.openIn(window.currentWebView)
                            }
                        }
                        onFeaturePermissionRequested: {
                            permBar.securityOrigin = securityOrigin;
                            permBar.requestedFeature = feature;
                            permBar.visible = true;
                        }
                        extraContextMenuEntriesComponent: ContextMenuExtras {}
                    }
                }

                Rectangle {
                    id: findBar
                    anchors.top: webEngineView.top
                    anchors.right: webEngineView.right
                    width: 240
                    height: 35
                    border.color: "lightgray"
                    border.width: 1
                    radius: 5
                    visible: false
                    color: browserWindow.color

                    RowLayout {
                        anchors.centerIn: findBar
                        TextField {
                            id: findTextField
                            onAccepted: {
                                webEngineView.experimental.findText(text, 0)
                            }
                        }
                        ToolButton {
                            id: findBackwardButton
                            iconSource: "icons/go-previous.png"
                            onClicked: webEngineView.experimental.findText(findTextField.text, WebEngineViewExperimental.FindBackward)
                        }
                        ToolButton {
                            id: findForwardButton
                            iconSource: "icons/go-next.png"
                            onClicked: webEngineView.experimental.findText(findTextField.text, 0)
                        }
                        ToolButton {
                            id: findCancelButton
                            iconSource: "icons/process-stop.png"
                            onClicked: findBar.visible = false
                        }
                    }
                }
            }
        }
    }
}
