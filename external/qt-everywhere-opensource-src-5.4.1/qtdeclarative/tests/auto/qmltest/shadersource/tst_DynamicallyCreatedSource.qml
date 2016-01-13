/****************************************************************************
**
** Copyright (C) 2014 Gunnar Sletta <gunnar@sletta.org>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
