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
import QtQuick.Controls 1.0
import QtQuick.Controls.Styles 1.0
import QtWebEngine 1.0
import QtQuick.Layouts 1.0

Rectangle {
    id: downloadView
    color: "lightgray"

    ListModel {
        id: downloadModel
        property var downloads: []
    }

    function append(download) {
        downloadModel.append(download)
        downloadModel.downloads.push(download)
    }

    Component {
        id: downloadItemDelegate

        Rectangle {
            width: listView.width
            height: childrenRect.height
            anchors.margins: 10
            radius: 3
            color: "transparent"
            border.color: "black"
            Rectangle {
                id: progressBar

                property real progress: downloadModel.downloads[index]
                                       ? downloadModel.downloads[index].receivedBytes / downloadModel.downloads[index].totalBytes : 0

                radius: 3
                color: width == listView.width ? "green" : "#2b74c7"
                width: listView.width * progress
                height: cancelButton.height

                Behavior on width {
                    SmoothedAnimation { duration: 100 }
                }
            }
            Rectangle {
                anchors {
                    left: parent.left
                    right: parent.right
                    leftMargin: 20
                }
                Label {
                    id: label
                    text: path
                    anchors {
                        verticalCenter: cancelButton.verticalCenter
                        left: parent.left
                        right: cancelButton.left
                    }
                }
                Button {
                    id: cancelButton
                    anchors.right: parent.right
                    iconSource: "icons/process-stop.png"
                    onClicked: {
                        var download = downloadModel.downloads[index]

                        download.cancel()

                        downloadModel.downloads = downloadModel.downloads.filter(function (el) {
                            return el.id !== download.id;
                        });
                        downloadModel.remove(index)
                    }
                }
            }
        }

    }
    ListView {
        id: listView
        anchors {
            topMargin: 10
            top: parent.top
            bottom: parent.bottom
            horizontalCenter: parent.horizontalCenter
        }
        width: parent.width - 20
        spacing: 5

        model: downloadModel
        delegate: downloadItemDelegate

        Text {
            visible: !listView.count
            horizontalAlignment: Text.AlignHCenter
            height: 30
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
            }
            font.pixelSize: 20
            text: "No active downloads."
        }

        Rectangle {
            color: "gray"
            anchors {
                bottom: parent.bottom
                left: parent.left
                right: parent.right
            }
            height: 30
            Button {
                id: okButton
                text: "OK"
                anchors.centerIn: parent
                onClicked: {
                    downloadView.visible = false
                }
            }
        }
    }
}
