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
    width: 150
    height: 150

    Custom3DLabel {
        id: initial
    }

    Custom3DLabel {
        id: initialized
        backgroundColor: "red"
        backgroundEnabled: false
        borderEnabled: false
        facingCamera: true
        font.family: "Times New Roman"
        text: "test label"
        textColor: "blue"

        position: Qt.vector3d(1.0, 0.5, 1.0)
        positionAbsolute: true
        rotation: Qt.quaternion(1, 0.5, 0, 0)
        scaling: Qt.vector3d(0.2, 0.2, 0.2)
        shadowCasting: true
        visible: false
    }

    Custom3DLabel {
        id: change
    }

    TestCase {
        name: "Custom3DLabel Initial"

        function test_initial() {
            compare(initial.backgroundColor, "#a0a0a4")
            compare(initial.backgroundEnabled, true)
            compare(initial.borderEnabled, true)
            compare(initial.facingCamera, false)
            compare(initial.font.family, "Arial")
            compare(initial.text, "")
            compare(initial.textColor, "#ffffff")

            compare(initial.meshFile, ":/defaultMeshes/plane")
            compare(initial.position, Qt.vector3d(0.0, 0.0, 0.0))
            compare(initial.positionAbsolute, false)
            compare(initial.rotation, Qt.quaternion(0, 0, 0, 0))
            compare(initial.scaling, Qt.vector3d(0.1, 0.1, 0.1))
            compare(initial.scalingAbsolute, true)
            compare(initial.shadowCasting, false)
            compare(initial.textureFile, "")
            compare(initial.visible, true)
        }
    }

    TestCase {
        name: "Custom3DLabel Initialized"

        function test_initialized() {
            compare(initialized.backgroundColor, "#ff0000")
            compare(initialized.backgroundEnabled, false)
            compare(initialized.borderEnabled, false)
            compare(initialized.facingCamera, true)
            compare(initialized.font.family, "Times New Roman")
            compare(initialized.text, "test label")
            compare(initialized.textColor, "#0000ff")

            compare(initialized.position, Qt.vector3d(1.0, 0.5, 1.0))
            compare(initialized.positionAbsolute, true)
            compare(initialized.rotation, Qt.quaternion(1, 0.5, 0, 0))
            compare(initialized.scaling, Qt.vector3d(0.2, 0.2, 0.2))
            compare(initialized.shadowCasting, true)
            compare(initialized.visible, false)
        }
    }

    TestCase {
        name: "Custom3DLabel Change"

        function test_change() {
            change.backgroundColor = "red"
            change.backgroundEnabled = false
            change.borderEnabled = false
            change.facingCamera = true
            change.font.family = "Times New Roman"
            change.text = "test label"
            change.textColor = "blue"

            change.position = Qt.vector3d(1.0, 0.5, 1.0)
            change.positionAbsolute = true
            change.rotation = Qt.quaternion(1, 0.5, 0, 0)
            change.scaling = Qt.vector3d(0.2, 0.2, 0.2)
            change.shadowCasting = true
            change.visible = false

            compare(change.backgroundColor, "#ff0000")
            compare(change.backgroundEnabled, false)
            compare(change.borderEnabled, false)
            compare(change.facingCamera, true)
            compare(change.font.family, "Times New Roman")
            compare(change.text, "test label")
            compare(change.textColor, "#0000ff")

            compare(change.position, Qt.vector3d(1.0, 0.5, 1.0))
            compare(change.positionAbsolute, true)
            compare(change.rotation, Qt.quaternion(1, 0.5, 0, 0))
            compare(change.scaling, Qt.vector3d(0.2, 0.2, 0.2))
            compare(change.shadowCasting, true)
            compare(change.visible, false)
        }
    }
}
