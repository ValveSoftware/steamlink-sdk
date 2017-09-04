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

import QtQuick 2.0
import QtTest 1.1

Item {
    id: top
    ListModel { id: emptymodel }
    ListModel { id: manyitems }
    ListModel { id: insertmodel }
    ListModel { id: move; ListElement { name: "Element0" } ListElement { name: "Element1" } }
    ListModel { id: firstmodel; ListElement { name: "FirstModelElement0" } }
    ListModel { id: secondmodel; ListElement { name: "SecondModelElement0" } ListElement { name: "SecondModelElement1" } }
    ListModel { id: altermodel; ListElement { name: "AlterModelElement0" } ListElement { name: "AlterModelElement1" } }

    TestCase {
        name: "ListModel"

        function test_empty() {
            compare(emptymodel.count, 0)
            emptymodel.clear();
            compare(emptymodel.count, 0)
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
        }

        function test_insert() {
            insertmodel.insert(0, {"name": "Element0"})
            compare(insertmodel.get(0).name, "Element0")
            insertmodel.insert(1, {"name": "Element1"})
            compare(insertmodel.get(1).name, "Element1")
        }

        function test_altermodeled() {
            tryCompare(altermodel, 'count', 2)
            compare(altermodel.get(0).name, "AlterModelElement0")
            compare(altermodel.get(1).name, "AlterModelElement1")
            altermodel.append({"name":"AlterModelElement2"})
            tryCompare(altermodel, 'count', 3)
            compare(altermodel.get(0).name, "AlterModelElement0")
            compare(altermodel.get(1).name, "AlterModelElement1")
            compare(altermodel.get(2).name, "AlterModelElement2")
            altermodel.insert(2,{"name":"AlterModelElement1.5"})
            tryCompare(altermodel, 'count', 4)
            compare(altermodel.get(0).name, "AlterModelElement0")
            compare(altermodel.get(1).name, "AlterModelElement1")
            compare(altermodel.get(2).name, "AlterModelElement1.5")
            compare(altermodel.get(3).name, "AlterModelElement2")
            tryCompare(altermodel, 'count', 4)
            altermodel.move(2,1,1);
            compare(altermodel.get(0).name, "AlterModelElement0")
            compare(altermodel.get(1).name, "AlterModelElement1.5")
            compare(altermodel.get(2).name, "AlterModelElement1")
            compare(altermodel.get(3).name, "AlterModelElement2")
            altermodel.remove(1,2)
            tryCompare(altermodel, 'count', 2)
            compare(altermodel.get(0).name, "AlterModelElement0")
            compare(altermodel.get(1).name, "AlterModelElement2")
            altermodel.set(1,{"name":"AlterModelElement1"})
            compare(altermodel.get(0).name, "AlterModelElement0")
            compare(altermodel.get(1).name, "AlterModelElement1")
            altermodel.setProperty(0, "name", "AlteredProperty")
            compare(altermodel.get(0).name, "AlteredProperty")
            altermodel.clear()
            tryCompare(altermodel, 'count', 0)
            compare(altermodel.get(0), undefined)
        }
    }
}
