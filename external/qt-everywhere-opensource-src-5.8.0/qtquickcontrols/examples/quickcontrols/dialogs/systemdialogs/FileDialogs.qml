/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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
import QtQuick.Controls 1.2
import QtQuick.Dialogs 1.1
import QtQuick.Layouts 1.1
import QtQuick.Window 2.0

Item {
    width: 580
    height: 400
    SystemPalette { id: palette }
    clip: true

    //! [filedialog]
    FileDialog {
        id: fileDialog
        visible: fileDialogVisible.checked
        modality: fileDialogModal.checked ? Qt.WindowModal : Qt.NonModal
        title: fileDialogSelectFolder.checked ? "Choose a folder" :
            (fileDialogSelectMultiple.checked ? "Choose some files" : "Choose a file")
        selectExisting: fileDialogSelectExisting.checked
        selectMultiple: fileDialogSelectMultiple.checked
        selectFolder: fileDialogSelectFolder.checked
        nameFilters: [ "Image files (*.png *.jpg)", "All files (*)" ]
        selectedNameFilter: "All files (*)"
        sidebarVisible: fileDialogSidebarVisible.checked
        onAccepted: {
            console.log("Accepted: " + fileUrls)
            if (fileDialogOpenFiles.checked)
                for (var i = 0; i < fileUrls.length; ++i)
                    Qt.openUrlExternally(fileUrls[i])
        }
        onRejected: { console.log("Rejected") }
    }
    //! [filedialog]

    ScrollView {
        id: scrollView
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            bottom: bottomBar.top
            leftMargin: 12
        }
        ColumnLayout {
            spacing: 8
            Item { Layout.preferredHeight: 4 } // padding
            Label {
                font.bold: true
                text: "File dialog properties:"
            }
            CheckBox {
                id: fileDialogModal
                text: "Modal"
                checked: true
                Binding on checked { value: fileDialog.modality != Qt.NonModal }
            }
            CheckBox {
                id: fileDialogSelectFolder
                text: "Select Folder"
                Binding on checked { value: fileDialog.selectFolder }
            }
            CheckBox {
                id: fileDialogSelectExisting
                text: "Select Existing Files"
                checked: true
                Binding on checked { value: fileDialog.selectExisting }
            }
            CheckBox {
                id: fileDialogSelectMultiple
                text: "Select Multiple Files"
                Binding on checked { value: fileDialog.selectMultiple }
            }
            CheckBox {
                id: fileDialogOpenFiles
                text: "Open Files After Accepting"
            }
            CheckBox {
                id: fileDialogSidebarVisible
                text: "Show Sidebar"
                checked: fileDialog.sidebarVisible
                Binding on checked { value: fileDialog.sidebarVisible }
            }
            CheckBox {
                id: fileDialogVisible
                text: "Visible"
                Binding on checked { value: fileDialog.visible }
            }
            Label {
                text: "<b>current view folder:</b> " + fileDialog.folder
            }
            Label {
                text: "<b>name filters:</b> {" + fileDialog.nameFilters + "}"
            }
            Label {
                text: "<b>current filter:</b>" + fileDialog.selectedNameFilter
            }
            Label {
                text: "<b>chosen files:</b> " + fileDialog.fileUrls
            }
            Label {
                text: "<b>chosen single path:</b> " + fileDialog.fileUrl
            }
        }
    }

    Rectangle {
        id: bottomBar
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: buttonRow.height * 1.2
        color: Qt.darker(palette.window, 1.1)
        border.color: Qt.darker(palette.window, 1.3)
        Row {
            id: buttonRow
            spacing: 6
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 12
            height: implicitHeight
            width: parent.width
            Button {
                text: "Open"
                anchors.verticalCenter: parent.verticalCenter
                onClicked: fileDialog.open()
            }
            Button {
                text: "Pictures"
                tooltip: "go to my Pictures directory"
                anchors.verticalCenter: parent.verticalCenter
                enabled: fileDialog.shortcuts.hasOwnProperty("pictures")
                onClicked: fileDialog.folder = fileDialog.shortcuts.pictures
            }
            Button {
                text: "Home"
                tooltip: "go to my home directory"
                anchors.verticalCenter: parent.verticalCenter
                enabled: fileDialog.shortcuts.hasOwnProperty("home")
                onClicked: fileDialog.folder = fileDialog.shortcuts.home
            }
        }
    }
}
