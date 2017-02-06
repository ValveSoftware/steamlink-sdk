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
import QtTest 1.1

Item {
    id: top
    height: 150
    width: 150

    // Scene3D is uncreatable, so it needs to be accessed via a graph
    Bars3D {
        id: initial
    }

    Bars3D {
        id: initialized
        scene.activeCamera: Camera3D { zoomLevel: 200 }
        scene.devicePixelRatio: 2.0
        scene.graphPositionQuery: Qt.point(0, 0)
        scene.primarySubViewport: Qt.rect(0, 0, 50, 50)
        scene.secondarySubViewport: Qt.rect(50, 50, 100, 100)
        scene.secondarySubviewOnTop: false
        scene.selectionQueryPosition: Qt.point(0, 0)
        scene.slicingActive: true
    }

    Bars3D {
        id: change
    }

    Bars3D {
        id: invalid
    }

    TestCase {
        name: "Scene3D Initial"

        function test_initial() {
            verify(initial.scene.activeCamera)
            verify(initial.scene.activeLight)
            compare(initial.scene.devicePixelRatio, 1.0)
            compare(initial.scene.graphPositionQuery, Qt.point(-1, -1))
            compare(initial.scene.invalidSelectionPoint, Qt.point(-1, -1))
            // TODO: subviewports are not set (QTRD-1807)
            //compare(initial.scene.primarySubViewport.x, 0)
            //compare(initial.scene.primarySubViewport.y, 0)
            //compare(initial.scene.primarySubViewport.width, 0)
            //compare(initial.scene.primarySubViewport.height, 0)
            // For some reason comparing like this fails in 5.8.0 (QRect vs. QRectF)
            //compare(initial.scene.primarySubViewport, Qt.rect(0, 0, 0, 0))
            // TODO: subviewports are not set (QTRD-1807)
            //compare(initial.scene.secondarySubViewport.x, 0)
            //compare(initial.scene.secondarySubViewport.y, 0)
            //compare(initial.scene.secondarySubViewport.width, 0)
            //compare(initial.scene.secondarySubViewport.height, 0)
            // For some reason comparing like this fails in 5.8.0 (QRect vs. QRectF)
            //compare(initial.scene.secondarySubViewport, Qt.rect(0, 0, 0, 0))
            compare(initial.scene.secondarySubviewOnTop, true)
            compare(initial.scene.selectionQueryPosition, Qt.point(-1, -1))
            compare(initial.scene.slicingActive, false)
            // TODO: viewport is not set by subviewports (QTRD-1807)
            //compare(initial.scene.viewport.x, 0)
            //compare(initial.scene.viewport.y, 0)
            //compare(initial.scene.viewport.width, 0)
            //compare(initial.scene.viewport.height, 0)
            // For some reason comparing like this fails in 5.8.0 (QRect vs. QRectF)
            //compare(initial.scene.viewport, Qt.rect(0, 0, 0, 0))
        }
    }

    TestCase {
        name: "Scene3D Initialized"

        function test_initialized() {
            compare(initialized.scene.activeCamera.zoomLevel, 200)
            compare(initialized.scene.devicePixelRatio, 2.0)
            compare(initialized.scene.graphPositionQuery, Qt.point(0, 0))
            // TODO: subviewports are not set (QTRD-1807)
            //compare(initialized.scene.primarySubViewport.x, 0)
            //compare(initialized.scene.primarySubViewport.y, 0)
            //compare(initialized.scene.primarySubViewport.width, 50)
            //compare(initialized.scene.primarySubViewport.height, 50)
            // For some reason comparing like this fails in 5.8.0 (QRect vs. QRectF)
            //compare(initialized.scene.primarySubViewport, Qt.rect(0, 0, 50, 50))
            // TODO: subviewports are not set (QTRD-1807)
            //compare(initialized.scene.secondarySubViewport.x, 50)
            //compare(initialized.scene.secondarySubViewport.y, 50)
            //compare(initialized.scene.secondarySubViewport.width, 100)
            //compare(initialized.scene.secondarySubViewport.height, 100)
            // For some reason comparing like this fails in 5.8.0 (QRect vs. QRectF)
            //compare(initialized.scene.secondarySubViewport, Qt.rect(50, 50, 100, 100))
            compare(initialized.scene.secondarySubviewOnTop, false)
            compare(initialized.scene.selectionQueryPosition, Qt.point(0, 0))
            compare(initialized.scene.slicingActive, true)
            // TODO: viewport is not set by subviewports (QTRD-1807)
            //compare(initialized.scene.viewport.x, 50)
            //compare(initialized.scene.viewport.y, 50)
            //compare(initialized.scene.viewport.width, 100)
            //compare(initialized.scene.viewport.height, 100)
            // For some reason comparing like this fails in 5.8.0 (QRect vs. QRectF)
            //compare(initialized.scene.viewport, Qt.rect(0, 0, 100, 100))
        }
    }

    TestCase {
        name: "Scene3D Change"

        Camera3D {
            id: camera1
            zoomLevel: 200
        }

        function test_change() {
            change.scene.activeCamera = camera1
            change.scene.devicePixelRatio = 2.0
            change.scene.graphPositionQuery = Qt.point(0, 0)
            change.scene.primarySubViewport = Qt.rect(0, 0, 50, 50)
            change.scene.secondarySubViewport = Qt.rect(50, 50, 100, 100)
            change.scene.secondarySubviewOnTop = false
            change.scene.selectionQueryPosition = Qt.point(0, 0) // TODO: When doing signal checks, add tests to check that queries return something (asynchronously)
            change.scene.slicingActive = true

            compare(change.scene.activeCamera.zoomLevel, 200)
            compare(change.scene.devicePixelRatio, 2.0)
            compare(change.scene.graphPositionQuery, Qt.point(0, 0))
            // TODO: subviewports are not set (QTRD-1807)
            //compare(change.scene.primarySubViewport.x, 0)
            //compare(change.scene.primarySubViewport.y, 0)
            //compare(change.scene.primarySubViewport.width, 50)
            //compare(change.scene.primarySubViewport.height, 50)
            // For some reason comparing like this fails in 5.8.0 (QRect vs. QRectF)
            //compare(change.scene.primarySubViewport, Qt.rect(0, 0, 50, 50))
            // TODO: subviewports are not set (QTRD-1807)
            //compare(change.scene.secondarySubViewport.x, 50)
            //compare(change.scene.secondarySubViewport.y, 50)
            //compare(change.scene.secondarySubViewport.width, 100)
            //compare(change.scene.secondarySubViewport.height, 100)
            // For some reason comparing like this fails in 5.8.0 (QRect vs. QRectF)
            //compare(change.scene.secondarySubViewport, Qt.rect(50, 50, 100, 100))
            compare(change.scene.secondarySubviewOnTop, false)
            compare(change.scene.selectionQueryPosition, Qt.point(0, 0))
            compare(change.scene.slicingActive, true)
            // TODO: viewport is not set by subviewports (QTRD-1807)
            //compare(change.scene.viewport.x, 0)
            //compare(change.scene.viewport.y, 0)
            //compare(change.scene.viewport.width, 100)
            //compare(change.scene.viewport.height, 100)
            // For some reason comparing like this fails in 5.8.0 (QRect vs. QRectF)
            //compare(change.scene.viewport, Qt.rect(0, 0, 100, 100))
        }
    }

    TestCase {
        name: "Scene3D Invalid"

        function test_invalid() {
            invalid.scene.primarySubViewport = Qt.rect(0, 0, -50, -50)
            compare(invalid.scene.primarySubViewport.x, 0)
            compare(invalid.scene.primarySubViewport.y, 0)
            compare(invalid.scene.primarySubViewport.width, 0)
            compare(invalid.scene.primarySubViewport.height, 0)
            // For some reason comparing like this fails in 5.8.0 (QRect vs. QRectF)
            //compare(change.scene.primarySubViewport, Qt.rect(0, 0, 0, 0))
        }
    }
}
