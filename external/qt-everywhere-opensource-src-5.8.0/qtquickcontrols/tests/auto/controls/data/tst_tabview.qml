/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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
import QtTest 1.0

Item {
    id: container
    width: 300
    height: 300

TestCase {
    id: testCase
    name: "Tests_TabView"
    when:windowShown
    width:400
    height:400

    function test_createTabView() {
        var tabView = Qt.createQmlObject('import QtQuick.Controls 1.2; TabView {}', testCase, '');
        tabView.destroy()
    }

    function test_repeater() {
        var tabView = Qt.createQmlObject('import QtQuick 2.2; import QtQuick.Controls 1.2; TabView { Repeater { model: 3; Tab { } } }', testCase, '');
        compare(tabView.count, 3)
        tabView.destroy()
    }

    Component {
        id: newTab
        Item {}
    }

    function test_changeIndex() {
        var tabView = Qt.createQmlObject('import QtQuick 2.2; import QtQuick.Controls 1.2; TabView { Repeater { model: 3; Tab { Text { text: index } } } }', testCase, '');
        compare(tabView.count, 3)
        verify(tabView.getTab(1).item == undefined)
        tabView.currentIndex = 1
        verify(tabView.getTab(1).item !== undefined)
        verify(tabView.getTab(2).item == undefined)
        tabView.currentIndex = 1
        verify(tabView.getTab(2).item !== undefined)
        tabView.destroy()
    }


    function test_addRemoveTab() {
        var tabView = Qt.createQmlObject('import QtQuick 2.2; import QtQuick.Controls 1.2; TabView { }', testCase, '');

        function verifyCurrentIndexCountDiff() {
            verify(!tabView.currentIndex || tabView.count > tabView.currentIndex)
        }
        tabView.currentIndexChanged.connect(verifyCurrentIndexCountDiff)
        tabView.countChanged.connect(verifyCurrentIndexCountDiff)

        compare(tabView.count, 0)
        compare(tabView.currentIndex, 0)
        tabView.addTab("title 1", newTab)
        compare(tabView.count, 1)
        compare(tabView.currentIndex, 0)
        tabView.addTab("title 2", newTab)
        compare(tabView.count, 2)
        compare(tabView.currentIndex, 0)
        compare(tabView.getTab(0).title, "title 1")
        compare(tabView.getTab(1).title, "title 2")

        tabView.currentIndex = 1

        tabView.insertTab(1, "title 3")
        compare(tabView.count, 3)
        compare(tabView.currentIndex, 2)
        compare(tabView.getTab(0).title, "title 1")
        compare(tabView.getTab(1).title, "title 3")
        compare(tabView.getTab(2).title, "title 2")

        tabView.insertTab(0, "title 4")
        compare(tabView.count, 4)
        compare(tabView.currentIndex, 3)
        compare(tabView.getTab(0).title, "title 4")
        compare(tabView.getTab(1).title, "title 1")
        compare(tabView.getTab(2).title, "title 3")
        compare(tabView.getTab(3).title, "title 2")

        tabView.insertTab(tabView.count, "title 5")
        compare(tabView.count, 5)
        compare(tabView.currentIndex, 3)
        compare(tabView.getTab(0).title, "title 4")
        compare(tabView.getTab(1).title, "title 1")
        compare(tabView.getTab(2).title, "title 3")
        compare(tabView.getTab(3).title, "title 2")
        compare(tabView.getTab(4).title, "title 5")

        tabView.removeTab(tabView.count - 1)
        compare(tabView.count, 4)
        compare(tabView.currentIndex, 3)
        compare(tabView.getTab(0).title, "title 4")
        compare(tabView.getTab(1).title, "title 1")
        compare(tabView.getTab(2).title, "title 3")
        compare(tabView.getTab(3).title, "title 2")

        tabView.removeTab(0)
        compare(tabView.count, 3)
        compare(tabView.currentIndex, 2)
        compare(tabView.getTab(0).title, "title 1")
        compare(tabView.getTab(1).title, "title 3")
        compare(tabView.getTab(2).title, "title 2")

        tabView.removeTab(1)
        compare(tabView.count, 2)
        compare(tabView.currentIndex, 1)
        compare(tabView.getTab(0).title, "title 1")
        compare(tabView.getTab(1).title, "title 2")

        tabView.removeTab(1)
        compare(tabView.count, 1)
        compare(tabView.currentIndex, 0)
        compare(tabView.getTab(0).title, "title 1")

        tabView.removeTab(0)
        compare(tabView.count, 0)
        compare(tabView.currentIndex, 0)
        tabView.destroy()
    }

    function test_moveTab_data() {
        return [
            {tag:"0->1 (0)", from: 0, to: 1, currentBefore: 0, currentAfter: 1},
            {tag:"0->1 (1)", from: 0, to: 1, currentBefore: 1, currentAfter: 0},
            {tag:"0->1 (2)", from: 0, to: 1, currentBefore: 2, currentAfter: 2},

            {tag:"0->2 (0)", from: 0, to: 2, currentBefore: 0, currentAfter: 2},
            {tag:"0->2 (1)", from: 0, to: 2, currentBefore: 1, currentAfter: 0},
            {tag:"0->2 (2)", from: 0, to: 2, currentBefore: 2, currentAfter: 1},

            {tag:"1->0 (0)", from: 1, to: 0, currentBefore: 0, currentAfter: 1},
            {tag:"1->0 (1)", from: 1, to: 0, currentBefore: 1, currentAfter: 0},
            {tag:"1->0 (2)", from: 1, to: 0, currentBefore: 2, currentAfter: 2},

            {tag:"1->2 (0)", from: 1, to: 2, currentBefore: 0, currentAfter: 0},
            {tag:"1->2 (1)", from: 1, to: 2, currentBefore: 1, currentAfter: 2},
            {tag:"1->2 (2)", from: 1, to: 2, currentBefore: 2, currentAfter: 1},

            {tag:"2->0 (0)", from: 2, to: 0, currentBefore: 0, currentAfter: 1},
            {tag:"2->0 (1)", from: 2, to: 0, currentBefore: 1, currentAfter: 2},
            {tag:"2->0 (2)", from: 2, to: 0, currentBefore: 2, currentAfter: 0},

            {tag:"2->1 (0)", from: 2, to: 1, currentBefore: 0, currentAfter: 0},
            {tag:"2->1 (1)", from: 2, to: 1, currentBefore: 1, currentAfter: 2},
            {tag:"2->1 (2)", from: 2, to: 1, currentBefore: 2, currentAfter: 1},

            {tag:"0->0", from: 0, to: 0, currentBefore: 0, currentAfter: 0},
            {tag:"-1->0", from: 0, to: 0, currentBefore: 1, currentAfter: 1},
            {tag:"0->-1", from: 0, to: 0, currentBefore: 2, currentAfter: 2},
            {tag:"1->10", from: 0, to: 0, currentBefore: 0, currentAfter: 0},
            {tag:"10->2", from: 0, to: 0, currentBefore: 1, currentAfter: 1},
            {tag:"10->-1", from: 0, to: 0, currentBefore: 2, currentAfter: 2}
        ]
    }

    function test_moveTab(data) {
        var tabView = Qt.createQmlObject('import QtQuick 2.2; import QtQuick.Controls 1.2; TabView { }', testCase, '');
        compare(tabView.count, 0)

        var titles = ["title 1", "title 2", "title 3"]

        var i = 0;
        for (i = 0; i < titles.length; ++i)
            tabView.addTab(titles[i], newTab)

        compare(tabView.count, titles.length)
        for (i = 0; i < tabView.count; ++i)
            compare(tabView.getTab(i).title, titles[i])

        tabView.currentIndex = data.currentBefore
        tabView.moveTab(data.from, data.to)

        compare(tabView.count, titles.length)
        compare(tabView.currentIndex, data.currentAfter)

        var title = titles[data.from]
        titles.splice(data.from, 1)
        titles.splice(data.to, 0, title)

        compare(tabView.count, titles.length)
        for (i = 0; i < tabView.count; ++i)
            compare(tabView.getTab(i).title, titles[i])

        tabView.destroy()
    }

    function test_dynamicTabs() {
        var test_tabView = '                                \
        import QtQuick 2.2;                                 \
        import QtQuick.Controls 1.2;                        \
        TabView {                                           \
            id: tabView;                                    \
            Tab { title: "static" }                         \
            property Component tabComponent: Component {    \
                id: tabComponent;                           \
                Tab { title: "dynamic" }                    \
            }                                               \
            Component.onCompleted: {                        \
                addTab("added", tabComponent);              \
                insertTab(0, "inserted", tabComponent);     \
                tabComponent.createObject(tabView);         \
            }                                               \
        }                                                   '

        var tabView = Qt.createQmlObject(test_tabView, testCase, '')
        // insertTab(), addTab(), createObject() and static Tab {}
        compare(tabView.count, 4)
        compare(tabView.getTab(0).title, "inserted")

        var tab = tabView.tabComponent.createObject(tabView)
        compare(tabView.count, 5)
        compare(tabView.getTab(4).title, "dynamic")
        tab.destroy()
        wait(0)
        compare(tabView.count, 4)
        tabView.destroy()
    }

    function test_dynamicModel() {
        var test_tabView = '                                \
        import QtQuick 2.2;                                 \
        import QtQuick.Controls 1.2;                        \
        TabView {                                           \
            id: tabView;                                    \
            property alias repeater: repeater;              \
            Repeater { id: repeater; Tab { } }              \
        }                                                   '

        var tabView = Qt.createQmlObject(test_tabView, testCase, '')
        compare(tabView.count, 0)

        tabView.repeater.model = 4
        compare(tabView.count, 4)

        tabView.repeater.model = 0
        compare(tabView.count, 0)

        tabView.destroy()
    }

    function test_mousePressOnTabBar() {
        var test_tabView = 'import QtQuick 2.2;             \
        import QtQuick.Controls 1.2;                        \
        Column {                                            \
            property alias tabview: _tabview;               \
            property alias textfield: _textfield;           \
        TabView {                                           \
            id: _tabview;                                   \
            width: 200; height: 100;                        \
            property alias tab1: _tab1;                     \
            property alias tab2: _tab2;                     \
            property alias tab3: _tab3;                     \
            Tab {                                           \
                id: _tab1;                                  \
                title: "Tab1";                              \
                active: true;                               \
                Column {                                    \
                    objectName: "column1";                  \
                    property alias child1: _child1;         \
                    property alias child2: _child2;         \
                    anchors.fill: parent;                   \
                    TextField {                             \
                        id: _child1;                        \
                        text: "textfile 1 in Tab1";         \
                    }                                       \
                    TextField {                             \
                        id: _child2;                        \
                        text: "textfile 2 in Tab1";         \
                    }                                       \
                }                                           \
            }                                               \
            Tab {                                           \
                id: _tab2;                                  \
                title: "Tab2";                              \
                active: true;                               \
                Column {                                    \
                    objectName: "column2";                  \
                    property alias child3: _child3;         \
                    property alias child4: _child4;         \
                    anchors.fill: parent;                   \
                    TextField {                             \
                        id: _child3;                        \
                        text: "textfile 1 in Tab2";         \
                    }                                       \
                    TextField {                             \
                        id: _child4;                        \
                        text: "textfile 2 in Tab2";         \
                    }                                       \
                }                                           \
            }                                               \
            Tab {                                           \
                id: _tab3;                                  \
                title: "Tab3";                              \
                active: true;                               \
                Column {                                    \
                    objectName: "column3";                  \
                    property alias child5: _child5;         \
                    property alias child6: _child6;         \
                    anchors.fill: parent;                   \
                    Button {                                \
                        id: _child5;                        \
                        activeFocusOnTab: false;            \
                        text: "button 1 in Tab3";           \
                    }                                       \
                    Button {                                \
                        id: _child6;                        \
                        activeFocusOnTab: false;            \
                        text: "button 2 in Tab3";           \
                    }                                       \
                }                                           \
            }                                               \
        }                                                   \
        TextField {                                         \
            id: _textfield;                                 \
            text: "textfile outside of tabview";            \
        }                                                   \
        }                                                   '

        var item = Qt.createQmlObject(test_tabView, container, '')

        var textField = item.textfield
        verify(textField !== null)

        var tabView = item.tabview
        verify(tabView !== null)
        compare(tabView.count, 3)
        verify(tabView.tab1.status === Loader.Ready)
        verify(tabView.tab2.status === Loader.Ready)
        verify(tabView.tab3.status === Loader.Ready)
        waitForRendering(tabView)

        var column1 = getColumnItem(tabView.tab1, "column1")
        verify(column1 !== null)
        var column2 = getColumnItem(tabView.tab2, "column2")
        verify(column2 !== null)
        var column3 = getColumnItem(tabView.tab3, "column3")
        verify(column3 !== null)

        var child1 = column1.child1
        verify(child1 !== null)
        var child3 = column2.child3
        verify(child3 !== null)

        var tabbarItem = getTabBarItem(tabView)
        verify(tabbarItem !== null)

        var tabrowItem = getTabRowItem(tabbarItem)
        verify(tabrowItem !== null)

        var mouseareas = populateMouseAreaItems(tabrowItem)
        verify(mouseareas.length, 3)

        var tab1 = mouseareas[0]
        verify(tab1 !== null)
        //printGeometry(tab1)

        waitForRendering(tab1)
        mouseClick(tab1, tab1.width/2, tab1.height/2)
        verify(child1.activeFocus)

        var tab2 = mouseareas[1]
        verify(tab2 !== null)
        //printGeometry(tab2)

        waitForRendering(tab2)
        mouseClick(tab2, tab2.width/2, tab2.height/2)
        verify(child3.activeFocus)

        waitForRendering(tab1)
        mouseClick(tab1, tab1.width/2, tab1.height/2)
        verify(child1.activeFocus)

        waitForRendering(tab2)
        mouseClick(tab2, tab2.width/2, tab2.height/2)
        verify(child3.activeFocus)

        var tab3 = mouseareas[2]
        verify(tab3 !== null)
        //printGeometry(tab3)

        waitForRendering(tab3)
        mouseClick(tab3, tab3.width/2, tab3.height/2)
        verify(!tab3.activeFocus)
        verify(!textField.activeFocus)

        compare(tabView.currentIndex, 2)
        tab1.enabled = false
        mouseClick(tab1, tab1.width/2, tab1.height/2)
        compare(tabView.currentIndex, 2)
        mouseClick(tab2, tab2.width/2, tab2.height/2)
        compare(tabView.currentIndex, 1)
        tab2.enabled = false
        compare(tabView.currentIndex, 1)
        tab1.enabled = true
        mouseClick(tab1, tab1.width/2, tab1.height/2)
        compare(tabView.currentIndex, 0)
        tab2.enabled = true
        mouseClick(tab2, tab2.width/2, tab2.height/2)
        compare(tabView.currentIndex, 1)

        item.destroy()
    }

    function test_43701() {
        var test_tabView = '                                \
        import QtQuick 2.2;                                 \
        import QtQuick.Controls 1.3;                        \
        TabView {                                           \
            id: tabView;                                    \
            currentIndex: 2;                                \
            Tab {} Tab {} Tab {}                            \
        }                                                   '

        var tabView = Qt.createQmlObject(test_tabView, testCase, '')
        compare(tabView.count, 3)
        compare(tabView.currentIndex, 2)

        tabView.destroy()
    }

    function printGeometry(control) {
        console.log("printGeometry:" + control)
        console.log("x=" + control.x + ",y=" + control.y + ",w=" + control.width + ",h=" + control.height)
        var g = control.mapToItem(null, control.x, control.y, control.width, control.height)
        console.log("x=" + g.x + ",y=" + g.y + ",w=" + g.width + ",h=" + g.height)
    }

    function getTabBarItem(control) {
        for (var i = 0; i < control.children.length; i++) {
            if (control.children[i].objectName === 'tabbar')
                return control.children[i]
        }
        return null
    }

    function getTabRowItem(control) {
        for (var i = 0; i < control.children.length; i++) {
            if (control.children[i].objectName === 'tabrow')
                return control.children[i]
        }
        return null
    }

    function getColumnItem(control, name) {
        for (var i = 0; i < control.children.length; i++) {
            if (control.children[i].objectName === name)
                return control.children[i]
        }
        return null
    }

    function populateMouseAreaItems(control) {
        var value = new Array()
        for (var i = 0; i < control.children.length; i++) {
            var sub = control.children[i]
            for (var j = 0; j < sub.children.length; j++) {
                var ssub = sub.children[j]
                if (ssub.objectName === "mousearea")
                    value.push(ssub)
            }
        }
        return value
    }
}
}
