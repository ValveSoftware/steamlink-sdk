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

Grid {
    id: menu
    enabled: opacity > 0 ? true : false

    property bool horizontalOrientation: true
    property list <Button> buttons
    width: parent.width
    height: horizontalOrientation ? 40 : ((children.length > 0) ? children[0].height * children.length : 0)
    spacing: 0
    property string exclusiveButton: ""
    property bool exclusive: false
    property bool autoWidth: false
    opacity: 1
    rows: (horizontalOrientation) ? 1 : children.length
    columns: (horizontalOrientation) ? children.length : 1
    signal clicked(string button)

    onChildrenChanged: {
        resizeItems()
    }

    onExclusiveChanged: {
        if (exclusive){
            if (children.length > 0) exclusiveButton = children[0].text
        }
        else
            exclusiveButton = ""
    }

    onExclusiveButtonChanged:{
        if (exclusive) {
            for (var i = 0; i<children.length; i++){
                if (children[i].text == exclusiveButton){
                    children[i].checked = true
                    break
                }
            }
        }
    }

    function addItem(caption) {
        var button, myArray
        if (horizontalOrientation)
            button = Qt.createQmlObject ('import "style"; Button {height: menu.height; onClicked: {menu.itemClicked(text)} style: HMenuItemStyle {}}', menu)
        else
            button = Qt.createQmlObject ('import "style"; Button {height: 35; width: menu.width; onClicked: {menu.itemClicked(text)} style: VMenuItemStyle {}}', menu)
        button.text = caption

        myArray = new Array()
        for (var i = 0; i<children.length; i++){
            myArray.push(children[i])
        }
        myArray.push(button)
        children = myArray

        return button
    }

    function deleteItem(caption){
        var myArray

        myArray = new Array()
        for (var i = 0; i<children.length; i++){
            if (children[i].text != caption)
                myArray.push(children[i])
        }
        children = myArray
    }

    function clear() {
        children = []
        exclusiveButton = ""
    }

    function disableItem(caption){
        for (var i = 0; i<children.length; i++){
            if (children[i].text == caption){
                children[i].disable()
                break
            }
        }
    }

    function enableItem(caption){
        for (var i = 0; i<children.length; i++){
            if (children[i].text == caption){
                children[i].enable()
                break
            }
        }
    }

    function resizeItems(){
        if (horizontalOrientation) {
            for (var i = 0; i<children.length; i++)
                children[i].width = parent.width/children.length - spacing
        } else if (autoWidth) {
            if (children.length > 0) {
                var maxWidth = children[0].paintedWidth
                for (var i = 1; i<children.length; i++){
                    if (children[i].paintedWidth > maxWidth) {
                        maxWidth = children[i].paintedWidth
                    }
                }
                menu.width = maxWidth + 20
            }
            for (var i = 0; i < children.length; i++)
                children[i].width = menu.width;
        }
    }

    function itemClicked(text){
        if (exclusive && text != exclusiveButton) {
            for (var i = 0; i<children.length; i++){
                if (children[i].text == exclusiveButton){
                    children[i].checked = false
                    break
                }
            }
            exclusiveButton = text
        }
        clicked(text)
    }
}
