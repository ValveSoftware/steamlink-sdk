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
import QtLocation 5.3
import QtLocation.examples 5.0


//! [CategoryModel view 1]
ListView {
    id: root

    property bool showSave: true
    property bool showRemove: true
    property bool showChildren: true

    signal categoryClicked(variant category)
    signal editClicked(variant category)
//! [CategoryModel view 1]

    anchors.topMargin: 10
    anchors.left: parent.left
    anchors.leftMargin: 10
    anchors.right: parent.right
    anchors.rightMargin: 10

    clip: true
    snapMode: ListView.SnapToItem
    spacing: 5

//! [CategoryModel view 2]
    header: IconButton {
        source: "../../resources/left.png"
        pressedSource: "../../resources/left_pressed.png"
        onClicked: categoryListModel.rootIndex = categoryListModel.parentModelIndex()
    }
//! [CategoryModel view 2]

//! [CategoryModel view 3]
    model: VisualDataModel {
        id: categoryListModel
        model: categoryModel
        delegate: CategoryDelegate {
            id: categoryDelegate

            showSave: root.showSave
            showRemove: root.showRemove
            showChildren: root.showChildren

            onClicked: root.categoryClicked(category);
            onArrowClicked: categoryListModel.rootIndex = categoryListModel.modelIndex(index)
            onCrossClicked: category.remove();
            onEditClicked: root.editClicked(category);
        }
    }
}
//! [CategoryModel view 3]
