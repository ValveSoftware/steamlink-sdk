/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Virtual Keyboard module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.1

Item {
    property bool active: listView.currentIndex != -1
    property int highlightIndex: -1
    property alias listView: listView
    property int keyCode
    property point origin
    property bool uppercased: keyboard.uppercased
    signal clicked

    z: 1
    visible: active
    anchors.fill: parent

    ListModel {
        id: listModel
    }

    ListView {
        id: listView
        spacing: 0
        model: listModel
        delegate: keyboard.style.alternateKeysListDelegate
        highlight: keyboard.style.alternateKeysListHighlight ? keyboard.style.alternateKeysListHighlight : defaultHighlight
        highlightMoveDuration: 0
        highlightResizeDuration: 0
        keyNavigationWraps: true
        orientation: ListView.Horizontal
        height: keyboard.style.alternateKeysListItemHeight
        x: origin.x
        y: origin.y - height - keyboard.style.alternateKeysListBottomMargin
        Component {
            id: defaultHighlight
            Item {}
        }
    }

    Loader {
        id: backgroundLoader
        sourceComponent: keyboard.style.alternateKeysListBackground
        anchors.fill: listView
        z: -1
        states: State {
            name: "highlighted"
            when: highlightIndex !== -1 && highlightIndex === listView.currentIndex &&
                  backgroundLoader.item !== null && backgroundLoader.item.hasOwnProperty("currentItemHighlight")
            PropertyChanges {
                target: backgroundLoader.item
                currentItemHighlight: true
            }
        }
    }

    onClicked: {
        if (active && listView.currentIndex >= 0 && listView.currentIndex < listView.model.count) {
            var activeKey = listView.model.get(listView.currentIndex)
            InputContext.inputEngine.virtualKeyClick(keyCode, activeKey.text,
                                                     uppercased ? Qt.ShiftModifier : 0)
        }
    }

    function open(key, originX, originY) {
        keyCode = key.key
        var alternativeKeys = key.effectiveAlternativeKeys
        if (alternativeKeys.length > 0) {
            for (var i = 0; i < alternativeKeys.length; i++) {
                listModel.append({ "text": uppercased ? alternativeKeys[i].toUpperCase() : alternativeKeys[i] })
            }
            listView.width = keyboard.style.alternateKeysListItemWidth * listModel.count
            listView.forceLayout()
            highlightIndex = key.effectiveAlternativeKeysHighlightIndex
            if (highlightIndex === -1) {
                console.log("AlternativeKeys: active key \"" + key.text + "\" not found in alternativeKeys \"" + alternativeKeys + ".\"")
                highlightIndex = 0
            }
            listView.currentIndex = highlightIndex
            var currentItemOffset = (listView.currentIndex + 0.5) * keyboard.style.alternateKeysListItemWidth
            origin = Qt.point(Math.min(Math.max(keyboard.style.alternateKeysListLeftMargin, originX - currentItemOffset), width - listView.width - keyboard.style.alternateKeysListRightMargin), originY)
            if (backgroundLoader.item && backgroundLoader.item.hasOwnProperty("currentItemOffset")) {
                backgroundLoader.item.currentItemOffset = currentItemOffset
            }
        }
        return active
    }

    function move(mouseX) {
        var newIndex = listView.indexAt(Math.max(1, Math.min(listView.width - 1, mapToItem(listView, mouseX, 0).x)), 1)
        if (newIndex !== listView.currentIndex) {
            listView.currentIndex = newIndex
        }
    }

    function close() {
        listView.currentIndex = -1
        listModel.clear()
    }
}
