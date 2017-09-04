/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

import QtQuick 2.1
import QtTest 1.1

Item {
    id: top

    ListView {
        id: emptylist
        height: 20
        width: 50
    }

    ListView {
        id: singleElementList
        height: 20
        width: 50
        model: 1
        property real heightForDelegate: 100
        property real contentHeightOnDelegateResize
        delegate: Rectangle {
            height: singleElementList.heightForDelegate
            onHeightChanged: {
                singleElementList.forceLayout();
                singleElementList.contentHeightOnDelegateResize = singleElementList.contentHeight;
            }
        }
    }

    ListView {
        id: viewmanyitems
        model: manyitems
        delegate: Text { text: model.name }
    }

    ListView {
        id: modelchange
        model: firstmodel
        delegate: Text { text: model.name }
    }

    ListView {
        id: modelalter
        model: altermodel
        delegate: Text { text: model.name }
    }

    ListView {
        id: asyncLoaderCurrentIndexListView
        width: 360
        height: 360
        model: asyncLoaderCurrentIndexListModel

        currentIndex: 0

        delegate: Loader {
            width: asyncLoaderCurrentIndexListView.width
            height: asyncLoaderCurrentIndexListView.height

            source: component
            asynchronous: true
        }
    }

    ListView {
        id: asyncListViewLoaderView
        width: 360
        height: 360
        model: asyncListViewLoaderModel

        currentIndex: 0

        delegate: Loader {
            width: asyncListViewLoaderView.width
            height: asyncListViewLoaderView.height

            source: component
            asynchronous: true
        }
    }

    ListView {
        id: listViewDelegateModelAfterCreate
        anchors.fill: parent
        property int createdDelegates: 0
    }

    ListView
    {
        id: listInteractiveCurrentIndexEnforce
        width: 600
        height: 600

        snapMode: ListView.SnapOneItem
        orientation: ListView.Horizontal
        interactive: !currentItem.moving
        highlightRangeMode: ListView.StrictlyEnforceRange

        model: 4

        focus: true
        Keys.onPressed: if (event.key == Qt.Key_K) currentIndex = currentIndex + 1;

        delegate: Flickable {
            width: 600
            height: 600
            contentWidth: 600
            contentHeight: 1200

            MouseArea { anchors.fill: parent }
            Rectangle { anchors.fill: parent; color: index == 0 ? "red" : index == 1 ? "green" : index == 2 ? "blue" : "white" }
        }
    }

    Component {
        id: delegateModelAfterCreateComponent
        Rectangle {
            width: 140
            height: 140
            border.color: "black"
            color: "red"
            Component.onCompleted: listViewDelegateModelAfterCreate.createdDelegates++;
        }
    }

    ListModel { id: emptymodel }
    ListModel { id: manyitems }
    ListModel { id: firstmodel; ListElement { name: "FirstModelElement0" } }
    ListModel { id: secondmodel; ListElement { name: "SecondModelElement0" } ListElement { name: "SecondModelElement1" } }
    ListModel { id: altermodel; ListElement { name: "AlterModelElement0" } ListElement { name: "AlterModelElement1" } }
    ListModel {
        id: asyncLoaderCurrentIndexListModel
        ListElement { component: "data/asyncloadercurrentindex.qml" }
        ListElement { component: "data/asyncloadercurrentindex.qml" }
        ListElement { component: "data/asyncloadercurrentindex.qml" }
        ListElement { component: "data/asyncloadercurrentindex.qml" }
        ListElement { component: "data/asyncloadercurrentindex.qml" }
        ListElement { component: "data/asyncloadercurrentindex.qml" }
    }
    ListModel {
        id: asyncListViewLoaderModel
        ListElement { component: "data/asynclistviewloader.qml" }
        ListElement { component: "data/asynclistviewloader.qml" }
        ListElement { component: "data/asynclistviewloader.qml" }
        ListElement { component: "data/asynclistviewloader.qml" }
        ListElement { component: "data/asynclistviewloader.qml" }
    }

    TestCase {
        name: "ListView"
        when: windowShown

        function test_empty() {
            compare(emptylist.count, 0)
            emptylist.model = emptymodel;
            compare(emptylist.count, 0)
        }

        function test_multipleitems_data() {
            return [
                {
                    tag: "10items",
                    numitems: 10
                },
                {
                    tag: "100items",
                    numitems: 100
                },
                {
                    tag: "10000items",
                    numitems: 10000
                }
            ]
        }

        function test_multipleitems(row) {
            var i;
            manyitems.clear();
            compare(manyitems.count, 0)
            for (i = 0; i < row.numitems; ++i) {
                manyitems.append({"name":"Item"+i})
            }
            compare(manyitems.count, row.numitems)
            tryCompare(viewmanyitems, 'count', row.numitems)

        }

        function test_modelchange() {
            tryCompare(modelchange, 'count', 1)
            modelchange.currentIndex = 0;
            compare(modelchange.currentItem.text, "FirstModelElement0")
            modelchange.model = secondmodel;
            tryCompare(modelchange, 'count', 2)
            modelchange.currentIndex = 0;
            compare(modelchange.currentItem.text, "SecondModelElement0")
            modelchange.currentIndex = 1;
            compare(modelchange.currentItem.text, "SecondModelElement1")
        }

        function test_modelaltered() {
            tryCompare(modelalter, 'count', 2)
            modelalter.currentIndex = 0;
            compare(modelalter.currentItem.text, "AlterModelElement0")
            modelalter.currentIndex = 1;
            compare(modelalter.currentItem.text, "AlterModelElement1")
            altermodel.append({"name":"AlterModelElement2"})
            tryCompare(modelalter, 'count', 3)
            modelalter.currentIndex = 0;
            compare(modelalter.currentItem.text, "AlterModelElement0")
            modelalter.currentIndex = 1;
            compare(modelalter.currentItem.text, "AlterModelElement1")
            modelalter.currentIndex = 2;
            compare(modelalter.currentItem.text, "AlterModelElement2")
            altermodel.insert(2,{"name":"AlterModelElement1.5"})
            tryCompare(modelalter, 'count', 4)
            modelalter.currentIndex = 0;
            compare(modelalter.currentItem.text, "AlterModelElement0")
            modelalter.currentIndex = 1;
            compare(modelalter.currentItem.text, "AlterModelElement1")
            modelalter.currentIndex = 2;
            compare(modelalter.currentItem.text, "AlterModelElement1.5")
            modelalter.currentIndex = 3;
            compare(modelalter.currentItem.text, "AlterModelElement2")
            altermodel.move(2,1,1);
            tryCompare(modelalter, 'count', 4)
            modelalter.currentIndex = 0;
            compare(modelalter.currentItem.text, "AlterModelElement0")
            modelalter.currentIndex = 1;
            compare(modelalter.currentItem.text, "AlterModelElement1.5")
            modelalter.currentIndex = 2;
            compare(modelalter.currentItem.text, "AlterModelElement1")
            modelalter.currentIndex = 3;
            compare(modelalter.currentItem.text, "AlterModelElement2")
            altermodel.remove(1,2)
            tryCompare(modelalter, 'count', 2)
            modelalter.currentIndex = 0;
            compare(modelalter.currentItem.text, "AlterModelElement0")
            modelalter.currentIndex = 1;
            compare(modelalter.currentItem.text, "AlterModelElement2")
            altermodel.set(1,{"name":"AlterModelElement1"})
            modelalter.currentIndex = 0;
            compare(modelalter.currentItem.text, "AlterModelElement0")
            modelalter.currentIndex = 1;
            compare(modelalter.currentItem.text, "AlterModelElement1")
            altermodel.clear()
            modelalter.forceLayout()
            tryCompare(modelalter, 'count', 0)
            compare(modelalter.currentItem, null)
        }

        function test_asyncLoaderCurrentIndexChange() {
            skip("more details in QTBUG-53780")
            for (var i = 0; i < 500; i++) {
                asyncLoaderCurrentIndexListView.currentIndex = 0;
                asyncLoaderCurrentIndexListView.currentIndex = 1;
                asyncLoaderCurrentIndexListView.currentIndex = 2;
                asyncLoaderCurrentIndexListView.currentIndex = 3;
                asyncLoaderCurrentIndexListView.currentIndex = 4;
                asyncLoaderCurrentIndexListView.currentIndex = 5;
            }
            wait(1000)
        }

        function test_asyncListViewLoader() {
            skip("more details in QTBUG-53780")
            for (var i = 0; i < 50; i++) {
                wait(10);
                asyncListViewLoaderView.currentIndex = 0;
                asyncListViewLoaderView.currentIndex = 1;
                asyncListViewLoaderView.currentIndex = 2;
                asyncListViewLoaderView.currentIndex = 3;
                asyncListViewLoaderView.currentIndex = 4;
            }
        }

        function test_set_delegate_model_after_list_creation() {
            listViewDelegateModelAfterCreate.delegate = delegateModelAfterCreateComponent;
            listViewDelegateModelAfterCreate.model = 40;
            verify(listViewDelegateModelAfterCreate.createdDelegates > 0);
        }

        function test_listInteractiveCurrentIndexEnforce() {
            mousePress(listInteractiveCurrentIndexEnforce, 10, 50);
            wait(1); // because Flickable pays attention to velocity, we need some time between movements
            mouseMove(listInteractiveCurrentIndexEnforce, 10, 40);
            wait(1);
            mouseMove(listInteractiveCurrentIndexEnforce, 10, 30);
            wait(1);
            mouseMove(listInteractiveCurrentIndexEnforce, 10, 20);
            wait(1);
            mouseMove(listInteractiveCurrentIndexEnforce, 10, 10);
            compare(listInteractiveCurrentIndexEnforce.interactive, false);
            mouseRelease(listInteractiveCurrentIndexEnforce, 10, 10);
            tryCompare(listInteractiveCurrentIndexEnforce, "interactive", true);
            keyClick("k");
            compare(listInteractiveCurrentIndexEnforce.currentIndex, 1);
            tryCompare(listInteractiveCurrentIndexEnforce, "contentX", listInteractiveCurrentIndexEnforce.width);
        }

        function test_forceLayoutForContentHeight() {
            singleElementList.heightForDelegate = 200;
            compare(singleElementList.contentHeightOnDelegateResize, 200);
        }
    }
}
