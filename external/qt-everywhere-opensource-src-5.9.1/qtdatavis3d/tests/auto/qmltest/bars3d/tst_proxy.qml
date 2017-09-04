/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
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
import QtDataVisualization 1.2
import QtTest 1.0

Item {
    id: top
    height: 150
    width: 150

    ItemModelBarDataProxy {
        id: initial
    }

    ItemModelBarDataProxy {
        id: initialized

        autoColumnCategories: false
        autoRowCategories: false
        columnCategories: ["colcat1", "colcat2"]
        columnRole: "col"
        columnRolePattern: /^.*-(\d\d)$/
        columnRoleReplace: "\\1"
        itemModel: ListModel { objectName: "model1" }
        multiMatchBehavior: ItemModelBarDataProxy.MMBAverage
        rotationRole: "rot"
        rotationRolePattern: /-/
        rotationRoleReplace: "\\1"
        rowCategories: ["rowcat1", "rowcat2"]
        rowRole: "row"
        rowRolePattern: /^(\d\d\d\d).*$/
        rowRoleReplace: "\\1"
        valueRole: "val"
        valueRolePattern: /-/
        valueRoleReplace: "\\1"

        columnLabels: ["col1", "col2"]
        rowLabels: ["row1", "row2"]
    }

    ItemModelBarDataProxy {
        id: change
    }

    TestCase {
        name: "ItemModelBarDataProxy Initial"

        function test_initial() {
            compare(initial.autoColumnCategories, true)
            compare(initial.autoRowCategories, true)
            compare(initial.columnCategories, [])
            compare(initial.columnRole, "")
            verify(initial.columnRolePattern)
            compare(initial.columnRoleReplace, "")
            verify(!initial.itemModel)
            compare(initial.multiMatchBehavior, ItemModelBarDataProxy.MMBLast)
            compare(initial.rotationRole, "")
            verify(initial.rotationRolePattern)
            compare(initial.rotationRoleReplace, "")
            compare(initial.rowCategories, [])
            compare(initial.rowRole, "")
            verify(initial.rowRolePattern)
            compare(initial.rowRoleReplace, "")
            compare(initial.useModelCategories, false)
            compare(initial.valueRole, "")
            verify(initial.valueRolePattern)
            compare(initial.valueRoleReplace, "")

            compare(initial.columnLabels.length, 0)
            compare(initial.rowCount, 0)
            compare(initial.rowLabels.length, 0)
            verify(!initial.series)

            compare(initial.type, AbstractDataProxy.DataTypeBar)
        }
    }

    TestCase {
        name: "ItemModelBarDataProxy Initialized"

        function test_initialized() {
            compare(initialized.autoColumnCategories, false)
            compare(initialized.autoRowCategories, false)
            compare(initialized.columnCategories.length, 2)
            compare(initialized.columnCategories[0], "colcat1")
            compare(initialized.columnCategories[1], "colcat2")
            compare(initialized.columnRole, "col")
            compare(initialized.columnRolePattern, /^.*-(\d\d)$/)
            compare(initialized.columnRoleReplace, "\\1")
            compare(initialized.itemModel.objectName, "model1")
            compare(initialized.multiMatchBehavior, ItemModelBarDataProxy.MMBAverage)
            compare(initialized.rotationRole, "rot")
            compare(initialized.rotationRolePattern, /-/)
            compare(initialized.rotationRoleReplace, "\\1")
            compare(initialized.rowCategories.length, 2)
            compare(initialized.rowCategories[0], "rowcat1")
            compare(initialized.rowCategories[1], "rowcat2")
            compare(initialized.rowRole, "row")
            compare(initialized.rowRolePattern, /^(\d\d\d\d).*$/)
            compare(initialized.rowRoleReplace, "\\1")
            compare(initialized.valueRole, "val")
            compare(initialized.valueRolePattern, /-/)
            compare(initialized.valueRoleReplace, "\\1")

            compare(initialized.columnLabels.length, 2)
            compare(initialized.rowCount, 2)
            compare(initialized.rowLabels.length, 2)
        }
    }

    TestCase {
        name: "ItemModelBarDataProxy Change"

        ListModel { id: model1; objectName: "model1" }

        function test_1_change() {
            change.autoColumnCategories = false
            change.autoRowCategories = false
            change.columnCategories = ["colcat1", "colcat2"]
            change.columnRole = "col"
            change.columnRolePattern = /^.*-(\d\d)$/
            change.columnRoleReplace = "\\1"
            change.itemModel = model1
            change.multiMatchBehavior = ItemModelBarDataProxy.MMBAverage
            change.rotationRole = "rot"
            change.rotationRolePattern = /-/
            change.rotationRoleReplace = "\\1"
            change.rowCategories = ["rowcat1", "rowcat2"]
            change.rowRole = "row"
            change.rowRolePattern = /^(\d\d\d\d).*$/
            change.rowRoleReplace = "\\1"
            change.useModelCategories = true // Overwrites columnLabels and rowLabels
            change.valueRole = "val"
            change.valueRolePattern = /-/
            change.valueRoleReplace = "\\1"

            change.columnLabels = ["col1", "col2"]
            change.rowLabels = ["row1", "row2"]
        }

        function test_2_test_change() {
            // This test has a dependency to the previous one due to asynchronous item model resolving
            compare(change.autoColumnCategories, false)
            compare(change.autoRowCategories, false)
            compare(change.columnCategories.length, 2)
            compare(change.columnCategories[0], "colcat1")
            compare(change.columnCategories[1], "colcat2")
            compare(change.columnRole, "col")
            compare(change.columnRolePattern, /^.*-(\d\d)$/)
            compare(change.columnRoleReplace, "\\1")
            compare(change.itemModel.objectName, "model1")
            compare(change.multiMatchBehavior, ItemModelBarDataProxy.MMBAverage)
            compare(change.rotationRole, "rot")
            compare(change.rotationRolePattern, /-/)
            compare(change.rotationRoleReplace, "\\1")
            compare(change.rowCategories.length, 2)
            compare(change.rowCategories[0], "rowcat1")
            compare(change.rowCategories[1], "rowcat2")
            compare(change.rowRole, "row")
            compare(change.rowRolePattern, /^(\d\d\d\d).*$/)
            compare(change.rowRoleReplace, "\\1")
            compare(change.useModelCategories, true)
            compare(change.valueRole, "val")
            compare(change.valueRolePattern, /-/)
            compare(change.valueRoleReplace, "\\1")

            compare(change.columnLabels.length, 1)
            compare(change.rowCount, 0)
            compare(change.rowLabels.length, 0)
        }
    }

    TestCase {
        name: "ItemModelBarDataProxy MultiMatchBehaviour"

        Bars3D {
            id: bars1

            Bar3DSeries {
                ItemModelBarDataProxy {
                    id: barProxy
                    itemModel: ListModel {
                        ListElement{ coords: "0,0"; data: "5"; }
                        ListElement{ coords: "0,0"; data: "15"; }
                    }
                    rowRole: "coords"
                    columnRole: "coords"
                    valueRole: "data"
                    rowRolePattern: /(\d),\d/
                    columnRolePattern: /(\d),(\d)/
                    rowRoleReplace: "\\1"
                    columnRoleReplace: "\\2"
                }
            }
        }

        function test_0_async_dummy() {
        }

        function test_1_test_multimatch() {
            compare(bars1.valueAxis.max, 15)
        }

        function test_2_multimatch() {
            barProxy.multiMatchBehavior = ItemModelBarDataProxy.MMBFirst
        }

        function test_3_test_multimatch() {
            compare(bars1.valueAxis.max, 5)
        }

        function test_4_multimatch() {
            barProxy.multiMatchBehavior = ItemModelBarDataProxy.MMBLast
        }

        function test_5_test_multimatch() {
            compare(bars1.valueAxis.max, 15)
        }

        function test_6_multimatch() {
            barProxy.multiMatchBehavior = ItemModelBarDataProxy.MMBAverage
        }

        function test_7_test_multimatch() {
            compare(bars1.valueAxis.max, 10)
        }

        function test_8_multimatch() {
            barProxy.multiMatchBehavior = ItemModelBarDataProxy.MMBCumulative
        }

        function test_9_test_multimatch() {
            compare(bars1.valueAxis.max, 20)
        }
    }
}
