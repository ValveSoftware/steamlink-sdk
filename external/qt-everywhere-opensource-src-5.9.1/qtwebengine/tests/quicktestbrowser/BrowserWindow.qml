/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.1
import QtWebEngine 1.2

import QtQuick.Controls 1.0
import QtQuick.Controls.Styles 1.0
import QtQuick.Layouts 1.0
import QtQuick.Window 2.1
import QtQuick.Controls.Private 1.0
import Qt.labs.settings 1.0
import QtQuick.Dialogs 1.2

ApplicationWindow {
    id: browserWindow
    property QtObject applicationRoot
    property Item currentWebView: tabs.currentIndex < tabs.count ? tabs.getTab(tabs.currentIndex).item.webView : null
    property int previousVisibility: Window.Windowed

    property bool isFullScreen: visibility == Window.FullScreen
    onIsFullScreenChanged: {
        // This is for the case where the system forces us to leave fullscreen.
        if (currentWebView && !isFullScreen) {
            currentWebView.state = ""
            if (currentWebView.isFullScreen) {
                currentWebView.fullScreenCancelled()
                fullScreenNotification.hide()
            }
        }
    }

    height: 600
    width: 800
    visible: true
    title: currentWebView && currentWebView.title

    Settings {
        id : appSettings
        property alias autoLoadImages: loadImages.checked;
        property alias javaScriptEnabled: javaScriptEnabled.checked;
        property alias errorPageEnabled: errorPageEnabled.checked;
        property alias pluginsEnabled: pluginsEnabled.checked;
    }

    // Make sure the Qt.WindowFullscreenButtonHint is set on OS X.
    Component.onCompleted: flags = flags | Qt.WindowFullscreenButtonHint

    // Create a styleItem to determine the platform.
    // When using style "mac", ToolButtons are not supposed to accept focus.
    StyleItem { id: styleItem }
    property bool platformIsMac: styleItem.style == "mac"

    Action {
        shortcut: "Ctrl+D"
        onTriggered: {
            downloadView.visible = !downloadView.visible
        }
    }

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
            tabs.createEmptyTab(currentWebView.profile)
            tabs.currentIndex = tabs.count - 1
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
                browserWindow.visibility = browserWindow.previousVisibility
        }
    }
    Action {
        shortcut: "Ctrl+0"
        onTriggered: zoomController.reset()
    }
    Action {
        shortcut: "Ctrl+-"
        onTriggered: zoomController.zoomOut()
    }
    Action {
        shortcut: "Ctrl+="
        onTriggered: zoomController.zoomIn()
    }

    Menu {
        id: backHistoryMenu

        Instantiator {
            model: currentWebView && currentWebView.navigationHistory.backItems
            MenuItem {
                text: model.title
                onTriggered: currentWebView.goBackOrForward(model.offset)
            }

            onObjectAdded: backHistoryMenu.insertItem(index, object)
            onObjectRemoved: backHistoryMenu.removeItem(object)
        }
    }

    Menu {
        id: forwardHistoryMenu

        Instantiator {
            model: currentWebView && currentWebView.navigationHistory.forwardItems
            MenuItem {
                text: model.title
                onTriggered: currentWebView.goBackOrForward(model.offset)
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
                            checked: true
                        }
                        MenuItem {
                            id: javaScriptEnabled
                            text: "JavaScript On"
                            checkable: true
                            checked: true
                        }
                        MenuItem {
                            id: errorPageEnabled
                            text: "ErrorPage On"
                            checkable: true
                            checked: true
                        }
                        MenuItem {
                            id: pluginsEnabled
                            text: "Plugins On"
                            checkable: true
                            checked: true
                        }
                        MenuItem {
                            id: offTheRecordEnabled
                            text: "Off The Record"
                            checkable: true
                            checked: currentWebView.profile.offTheRecord
                            onToggled: currentWebView.profile = checked ? otrProfile : testProfile;
                        }
                        MenuItem {
                            id: httpDiskCacheEnabled
                            text: "HTTP Disk Cache"
                            checkable: !currentWebView.profile.offTheRecord
                            checked: (currentWebView.profile.httpCacheType == WebEngineProfile.DiskHttpCache)
                            onToggled: currentWebView.profile.httpCacheType = checked ? WebEngineProfile.DiskHttpCache : WebEngineProfile.MemoryHttpCache;
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
        function createEmptyTab(profile) {
            var tab = addTab("", tabComponent)
            // We must do this first to make sure that tab.active gets set so that tab.item gets instantiated immediately.
            tab.active = true
            tab.title = Qt.binding(function() { return tab.item.title })
            tab.item.webView.profile = profile
            return tab
        }

        anchors.fill: parent
        Component.onCompleted: createEmptyTab(testProfile)

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
                    settings.autoLoadImages: appSettings.autoLoadImages
                    settings.javascriptEnabled: appSettings.javaScriptEnabled
                    settings.errorPageEnabled: appSettings.errorPageEnabled
                    settings.pluginsEnabled: appSettings.pluginsEnabled

                    onCertificateError: {
                        if (!acceptedCertificates.shouldAutoAccept(error)){
                            error.defer()
                            sslDialog.enqueue(error)
                        } else{
                            error.ignoreCertificateError()
                        }
                    }

                    onNewViewRequested: {
                        if (!request.userInitiated)
                            print("Warning: Blocked a popup window.")
                        else if (request.destination == WebEngineView.NewViewInTab) {
                            var tab = tabs.createEmptyTab(currentWebView.profile)
                            tabs.currentIndex = tabs.count - 1
                            request.openIn(tab.item.webView)
                        } else if (request.destination == WebEngineView.NewViewInBackgroundTab) {
                            var tab = tabs.createEmptyTab(currentWebView.profile)
                            request.openIn(tab.item.webView)
                        } else if (request.destination == WebEngineView.NewViewInDialog) {
                            var dialog = applicationRoot.createDialog(currentWebView.profile)
                            request.openIn(dialog.currentWebView)
                        } else {
                            var window = applicationRoot.createWindow(currentWebView.profile)
                            request.openIn(window.currentWebView)
                        }
                    }

                    onFullScreenRequested: {
                        if (request.toggleOn) {
                            webEngineView.state = "FullScreen"
                            browserWindow.previousVisibility = browserWindow.visibility
                            browserWindow.showFullScreen()
                            fullScreenNotification.show()
                        } else {
                            webEngineView.state = ""
                            browserWindow.visibility = browserWindow.previousVisibility
                            fullScreenNotification.hide()
                        }
                        request.accept()
                    }

                    onFeaturePermissionRequested: {
                        permBar.securityOrigin = securityOrigin;
                        permBar.requestedFeature = feature;
                        permBar.visible = true;
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
                                webEngineView.findText(text)
                            }
                        }
                        ToolButton {
                            id: findBackwardButton
                            iconSource: "icons/go-previous.png"
                            onClicked: webEngineView.findText(findTextField.text, WebEngineView.FindBackward)
                        }
                        ToolButton {
                            id: findForwardButton
                            iconSource: "icons/go-next.png"
                            onClicked: webEngineView.findText(findTextField.text)
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

    QtObject{
        id:acceptedCertificates

        property var acceptedUrls : []

        function shouldAutoAccept(certificateError){
            var domain = utils.domainFromString(certificateError.url)
            return acceptedUrls.indexOf(domain) >= 0
        }
    }

    MessageDialog {
        id: sslDialog

        property var certErrors: []
        icon: StandardIcon.Warning
        standardButtons: StandardButton.No | StandardButton.Yes
        title: "Server's certificate not trusted"
        text: "Do you wish to continue?"
        detailedText: "If you wish so, you may continue with an unverified certificate. " +
                      "Accepting an unverified certificate means " +
                      "you may not be connected with the host you tried to connect to.\n" +
                      "Do you wish to override the security check and continue?"
        onYes: {
            var cert = certErrors.shift()
            var domain = utils.domainFromString(cert.url)
            acceptedCertificates.acceptedUrls.push(domain)
            cert.ignoreCertificateError()
            presentError()
        }
        onNo: reject()
        onRejected: reject()

        function reject(){
            certErrors.shift().rejectCertificate()
            presentError()
        }
        function enqueue(error){
            certErrors.push(error)
            presentError()
        }
        function presentError(){
            visible = certErrors.length > 0
        }
    }

    FullScreenNotification {
        id: fullScreenNotification
    }

    DownloadView {
        id: downloadView
        visible: false
        anchors.fill: parent
    }

    function onDownloadRequested(download) {
        downloadView.visible = true
        downloadView.append(download)
        download.accept()
    }

    ZoomController {
      id: zoomController
      y: parent.mapFromItem(currentWebView, 0 , 0).y - 4
      anchors.right: parent.right
      width: (parent.width > 800) ? parent.width * 0.25 : 220
      anchors.rightMargin: (parent.width > 400) ? 100 : 0
    }
    Binding {
        target: currentWebView
        property: "zoomFactor"
        value: zoomController.zoomFactor
    }
}
