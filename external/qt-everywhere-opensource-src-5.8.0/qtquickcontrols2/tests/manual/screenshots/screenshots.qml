/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

import QtQuick 2.6
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.0
import Qt.labs.folderlistmodel 2.1

ApplicationWindow {
    id: window
    title: "Qt Quick Controls 2 - Screenshots"
    visible: true
    width: Math.max(600, loader.implicitWidth)
    height: Math.max(600, loader.implicitHeight)

    property string currentFilePath
    property string lastSavePath

    Shortcut {
        sequence: "Ctrl+Q"
        onActivated: Qt.quit()
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent

            ToolButton {
                text: "Choose Snippet"
                focusPolicy: Qt.NoFocus
                onClicked: snippetDrawer.open()
            }
        }
    }

    Drawer {
        id: snippetDrawer
        width: window.width / 2
        height: window.height

        ListView {
            id: snippetsListView
            anchors.fill: parent
            model: FolderListModel {
                folder: snippetsDir
                nameFilters: ["*.qml"]
                showDirs: false
            }
            delegate: ItemDelegate {
                width: parent.width
                text: fileName
                focusPolicy: Qt.NoFocus

                readonly property string baseName: fileBaseName

                contentItem: Label {
                    text: parent.text
                    elide: Text.ElideLeft
                }
                onClicked: {
                    snippetsListView.currentIndex = index;
                    loader.source = "file:///" + filePath;
                    currentFilePath = filePath;
                    snippetDrawer.close();
                }
            }
        }
    }

    Loader {
        id: loader
        anchors.centerIn: parent
    }

    ToolTip {
        id: saveResultToolTip
        x: window.contentItem.width / 2 - width / 2
        y: window.contentItem.height - height - 20
        timeout: 3000
    }

    footer: ToolBar {
        RowLayout {
            anchors.fill: parent

            ToolButton {
                text: "Open Output Folder"
                focusPolicy: Qt.NoFocus
                onClicked: Qt.openUrlExternally(screenshotsDir)
            }

            ToolButton {
                text: "Open Last Screenshot"
                focusPolicy: Qt.NoFocus
                enabled: lastSavePath.length > 0
                onClicked: Qt.openUrlExternally(lastSavePath)
            }

            Item {
                Layout.fillWidth: true
            }

            ToolButton {
                text: "Take Screenshot"
                focusPolicy: Qt.NoFocus
                enabled: loader.status === Loader.Ready
                onClicked: {
                    if (!loader.item)
                        return;

                    var grabSuccessful = loader.grabToImage(function(result) {
                        var savePath = screenshotsDirStr + "/" + snippetsListView.currentItem.baseName + ".png";
                        if (result.saveToFile(savePath)) {
                            saveResultToolTip.text = "Successfully saved screenshot to output folder";
                            lastSavePath = savePath;
                        } else {
                            saveResultToolTip.text = "Failed to save screenshot";
                        }
                    })
                    if (!grabSuccessful)
                        saveResultToolTip.text = "Failed to grab image";
                    saveResultToolTip.open();
                }
            }
        }
    }
}
