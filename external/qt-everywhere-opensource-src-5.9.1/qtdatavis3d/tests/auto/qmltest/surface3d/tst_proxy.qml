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

    ItemModelSurfaceDataProxy {
        id: initial
    }

    ItemModelSurfaceDataProxy {
        id: initialized

        autoColumnCategories: false
        autoRowCategories: false
        columnCategories: ["colcat1", "colcat2"]
        columnRole: "col"
        columnRolePattern: /^.*-(\d\d)$/
        columnRoleReplace: "\\1"
        itemModel: ListModel { objectName: "model1" }
        multiMatchBehavior: ItemModelSurfaceDataProxy.MMBAverage
        rowCategories: ["rowcat1", "rowcat2"]
        rowRole: "row"
        rowRolePattern: /^(\d\d\d\d).*$/
        rowRoleReplace: "\\1"
        xPosRole: "x"
        xPosRolePattern: /^.*-(\d\d)$/
        xPosRoleReplace: "\\1"
        yPosRole: "y"
        yPosRolePattern: /^(\d\d\d\d).*$/
        yPosRoleReplace: "\\1"
        zPosRole: "z"
        zPosRolePattern: /-/
        zPosRoleReplace: "\\1"
    }

    ItemModelSurfaceDataProxy {
        id: change
    }

    TestCase {
        name: "ItemModelSurfaceDataProxy Initial"

        function test_initial() {
            compare(initial.autoColumnCategories, true)
            compare(initial.autoRowCategories, true)
            compare(initial.columnCategories, [])
            compare(initial.columnRole, "")
            verify(initial.columnRolePattern)
            compare(initial.columnRoleReplace, "")
            verify(!initial.itemModel)
            compare(initial.multiMatchBehavior, ItemModelSurfaceDataProxy.MMBLast)
            compare(initial.rowCategories, [])
            compare(initial.rowRole, "")
            verify(initial.rowRolePattern)
            compare(initial.rowRoleReplace, "")
            compare(initial.useModelCategories, false)
            compare(initial.xPosRole, "")
            verify(initial.xPosRolePattern)
            compare(initial.xPosRoleReplace, "")
            compare(initial.yPosRole, "")
            verify(initial.yPosRolePattern)
            compare(initial.yPosRoleReplace, "")
            compare(initial.zPosRole, "")
            verify(initial.zPosRolePattern)
            compare(initial.zPosRoleReplace, "")

            compare(initial.columnCount, 0)
            compare(initial.rowCount, 0)
            verify(!initial.series)

            compare(initial.type, AbstractDataProxy.DataTypeSurface)
        }
    }

    TestCase {
        name: "ItemModelSurfaceDataProxy Initialized"

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
            compare(initialized.multiMatchBehavior, ItemModelSurfaceDataProxy.MMBAverage)
            compare(initialized.rowCategories.length, 2)
            compare(initialized.rowCategories[0], "rowcat1")
            compare(initialized.rowCategories[1], "rowcat2")
            compare(initialized.rowRole, "row")
            compare(initialized.rowRolePattern, /^(\d\d\d\d).*$/)
            compare(initialized.rowRoleReplace, "\\1")
            compare(initialized.xPosRole, "x")
            compare(initialized.xPosRolePattern, /^.*-(\d\d)$/)
            compare(initialized.xPosRoleReplace, "\\1")
            compare(initialized.yPosRole, "y")
            compare(initialized.yPosRolePattern, /^(\d\d\d\d).*$/)
            compare(initialized.yPosRoleReplace, "\\1")
            compare(initialized.zPosRole, "z")
            compare(initialized.zPosRolePattern, /-/)
            compare(initialized.zPosRoleReplace, "\\1")

            compare(initialized.columnCount, 2)
            compare(initialized.rowCount, 2)
        }
    }

    TestCase {
        name: "ItemModelSurfaceDataProxy Change"

        ListModel { id: model1; objectName: "model1" }

        function test_1_change() {
            change.autoColumnCategories = false
            change.autoRowCategories = false
            change.columnCategories = ["colcat1", "colcat2"]
            change.columnRole = "col"
            change.columnRolePattern = /^.*-(\d\d)$/
            change.columnRoleReplace = "\\1"
            change.itemModel = model1
            change.multiMatchBehavior = ItemModelSurfaceDataProxy.MMBAverage
            change.rowCategories = ["rowcat1", "rowcat2"]
            change.rowRole = "row"
            change.rowRolePattern = /^(\d\d\d\d).*$/
            change.rowRoleReplace = "\\1"
            change.useModelCategories = true // Overwrites columnLabels and rowLabels
            change.xPosRole = "x"
            change.xPosRolePattern = /^.*-(\d\d)$/
            change.xPosRoleReplace = "\\1"
            change.yPosRole = "y"
            change.yPosRolePattern = /^(\d\d\d\d).*$/
            change.yPosRoleReplace = "\\1"
            change.zPosRole = "z"
            change.zPosRolePattern = /-/
            change.zPosRoleReplace = "\\1"
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
            compare(change.multiMatchBehavior, ItemModelSurfaceDataProxy.MMBAverage)
            compare(change.rowCategories.length, 2)
            compare(change.rowCategories[0], "rowcat1")
            compare(change.rowCategories[1], "rowcat2")
            compare(change.rowRole, "row")
            compare(change.rowRolePattern, /^(\d\d\d\d).*$/)
            compare(change.rowRoleReplace, "\\1")
            compare(change.useModelCategories, true)
            compare(change.xPosRole, "x")
            compare(change.xPosRolePattern, /^.*-(\d\d)$/)
            compare(change.xPosRoleReplace, "\\1")
            compare(change.yPosRole, "y")
            compare(change.yPosRolePattern, /^(\d\d\d\d).*$/)
            compare(change.yPosRoleReplace, "\\1")
            compare(change.zPosRole, "z")
            compare(change.zPosRolePattern, /-/)
            compare(change.zPosRoleReplace, "\\1")

            compare(change.columnCount, 0)
            compare(change.rowCount, 0)
        }
    }

    TestCase {
        name: "ItemModelSurfaceDataProxy MultiMatchBehaviour"

        Surface3D {
            id: surface1
            Surface3DSeries {
                ItemModelSurfaceDataProxy {
                    id: surfaceProxy
                    itemModel: ListModel {
                        ListElement{ coords: "0,0"; data: "5"; }
                        ListElement{ coords: "0,0"; data: "15"; }
                        ListElement{ coords: "0,1"; data: "5"; }
                        ListElement{ coords: "0,1"; data: "15"; }
                        ListElement{ coords: "1,0"; data: "5"; }
                        ListElement{ coords: "1,0"; data: "15"; }
                        ListElement{ coords: "1,1"; data: "0"; }
                    }
                    rowRole: "coords"
                    columnRole: "coords"
                    yPosRole: "data"
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
            compare(surface1.axisY.max, 15)
        }

        function test_2_multimatch() {
            surfaceProxy.multiMatchBehavior = ItemModelSurfaceDataProxy.MMBFirst
        }

        function test_3_test_multimatch() {
            compare(surface1.axisY.max, 5)
        }

        function test_4_multimatch() {
            surfaceProxy.multiMatchBehavior = ItemModelSurfaceDataProxy.MMBLast
        }

        function test_5_test_multimatch() {
            compare(surface1.axisY.max, 15)
        }

        function test_6_multimatch() {
            surfaceProxy.multiMatchBehavior = ItemModelSurfaceDataProxy.MMBAverage
        }

        function test_7_test_multimatch() {
            compare(surface1.axisY.max, 10)
        }

        function test_8_multimatch() {
            surfaceProxy.multiMatchBehavior = ItemModelSurfaceDataProxy.MMBCumulativeY
        }

        function test_9_test_multimatch() {
            compare(surface1.axisY.max, 20)
        }
    }
}
