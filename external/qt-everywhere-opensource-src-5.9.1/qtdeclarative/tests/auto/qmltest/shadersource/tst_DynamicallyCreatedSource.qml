/****************************************************************************
**
** Copyright (C) 2016 Gunnar Sletta <gunnar@sletta.org>
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
import QtTest 1.1

Item {
    id: root

    width: 100
    height: 62

    Rectangle {
        id: rect
        anchors.fill: parent
        color: "red"
        visible: false
    }

    Component {
        id: component;
        ShaderEffectSource { anchors.fill: parent }
    }

    property var source: undefined;

    Timer {
        id: timer
        interval: 100
        running: true
        onTriggered: {
            var source = component.createObject();
            source.sourceItem = rect;
            source.parent = root;
            root.source = source;
        }
    }

    TestCase {
        id: testcase
        name: "shadersource-dynamic-shadersource"
        when: root.source != undefined

        function test_endresult() {
            var image = grabImage(root);
            compare(image.red(0,0), 255);
            compare(image.green(0,0), 0);
            compare(image.blue(0,0), 0);
        }

    }
}
