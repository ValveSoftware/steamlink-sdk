/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.2
import QtCanvas3D 1.0
import QtTest 1.0

import "tst_uniforms.js" as Content

Item {
    id: top
    height: 300
    width: 300

    Canvas3D {
        id: uniforms_test
        property bool heightChanged: false
        property bool widthChanged: false
        property int initStatus: -1
        property int renderStatus: -1

        anchors.fill: parent
        onInitializeGL: initStatus = Content.initializeGL(uniforms_test)
        onPaintGL: {
            renderStatus = Content.paintGL(uniforms_test)
        }
        onHeightChanged: heightChanged = true
        onWidthChanged: widthChanged = true
    }

    TestCase {
        name: "Canvas3D_test_uniforms"
        when: windowShown

        function test_uniforms() {
            waitForRendering(uniforms_test)
            tryCompare(uniforms_test, "initStatus", 0)
            tryCompare(uniforms_test, "renderStatus", 0)
        }
    }
}
