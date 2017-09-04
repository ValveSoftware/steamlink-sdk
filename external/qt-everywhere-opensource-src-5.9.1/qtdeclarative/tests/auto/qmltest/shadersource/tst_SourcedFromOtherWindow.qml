/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

import QtQuick 2.0
import QtQuick.Window 2.0

import QtTest 1.1

Item {
    Rectangle {
        id: box
        color: "red"
    }

    ShaderEffectSource {
        id: theSource
        sourceItem: box
    }

    Window {
        id: childWindow

        width: 100
        height: 100

        property bool rendered: false;
        visible: true
        onFrameSwapped: rendered = true;

        ShaderEffect {
            property variant source: theSource;
            anchors.fill: parent
        }
    }

    TestCase {
        name: "shadersource-from-other-window"
        when: childWindow.isRendered
        function test_endresult() {
            verify(true); // that we got here without problems...
        }
    }
}
