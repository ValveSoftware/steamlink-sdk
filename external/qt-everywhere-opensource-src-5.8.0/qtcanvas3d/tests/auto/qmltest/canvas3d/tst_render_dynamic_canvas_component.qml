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
import QtCanvas3D 1.1

import "tst_render_simple.js" as Content

Canvas3D {
    id: canvas3d
    anchors.fill: parent

    property var activeContent: Content
    property bool initOk: false
    property bool renderOk: false
    property bool contextLostOk: false
    property bool contextRestoredOk: false

    onInitializeGL: {
        initOk = activeContent.initializeGL(canvas3d)
    }
    onPaintGL: {
        renderOk = true
        activeContent.paintGL(canvas3d)
    }
    onContextLost: {
        contextLostOk = activeContent.checkContextLost();
    }

    onContextRestored: {
        contextRestoredOk = activeContent.checkContextRestored();
    }

    function checkContextLostError() {
        return activeContent.checkContextLostError();
    }
}
