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

import "tst_render_simple.js" as Content

Item {
    id: top
    height: 300
    width: 300

    Canvas3D {
        id: render_simple
        property bool heightChanged: false
        property bool widthChanged: false
        property bool initOk: false
        property bool renderOk: false
        height: 300
        width: 300
        onInitializeGL: initOk = Content.initializeGL(render_simple)
        onPaintGL: {
            renderOk = Content.paintGL(render_simple)
        }
        onResizeGL: Content.resizeGL(render_simple)
        onHeightChanged: heightChanged = true
        onWidthChanged: widthChanged = true
    }

    TestCase {
        name: "Canvas3D_render_simple"
        when: windowShown

        function test_render_1_simple() {
            waitForRendering(render_simple)
            tryCompare(render_simple, "initOk", true)
            tryCompare(render_simple, "renderOk", true)
        }

        function test_render_2_resize() {
            render_simple.heightChanged = false
            render_simple.widthChanged = false
            render_simple.renderOk = false
            render_simple.height = 200
            waitForRendering(render_simple)
            verify(render_simple.heightChanged === true)
            verify(render_simple.widthChanged === false)
            tryCompare(render_simple, "renderOk", true)
            var fboHeight = Content.getHeight()
            var fboWidth = Content.getWidth()
            verify(fboHeight === render_simple.height)
            verify(fboWidth === render_simple.width)

            render_simple.renderOk = false
            render_simple.width = 200
            waitForRendering(render_simple)
            verify(render_simple.heightChanged === true)
            verify(render_simple.widthChanged === true)
            tryCompare(render_simple, "renderOk", true)
            fboHeight = Content.getHeight()
            fboWidth = Content.getWidth()
            verify(fboHeight === render_simple.height)
            verify(fboWidth === render_simple.width)

            render_simple.heightChanged = false
            render_simple.widthChanged = false
            render_simple.width = 0
            render_simple.height = 0
            waitForRendering(render_simple)
            verify(render_simple.heightChanged === true)
            verify(render_simple.widthChanged === true)
            render_simple.renderOk = false
            waitForRendering(render_simple)
            tryCompare(render_simple, "renderOk", true)
            fboHeight = Content.getHeight()
            fboWidth = Content.getWidth()
            // Zero size canvas will still create 1x1 fbo
            verify(fboHeight === 1)
            verify(fboWidth === 1)
        }
    }
}
