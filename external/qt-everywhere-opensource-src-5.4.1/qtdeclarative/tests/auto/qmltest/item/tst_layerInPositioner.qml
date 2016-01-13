/****************************************************************************
**
** Copyright (C) 2014 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
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

import QtQuick 2.4
import QtTest 1.1

Item {
    id: root;
    width: 400
    height: 400

    TestCase {
        id: testCase
        name: "transparentForPositioner"
        when: windowShown
        function test_endresult() {
            var image = grabImage(root);

            // Row of red, green, blue and white box inside blue
            // At 10,10, spanning 10x10 pixels each
            verify(image.pixel(10, 10) == Qt.rgba(1, 0, 0, 1));
            verify(image.pixel(20, 10) == Qt.rgba(0, 1, 0, 1));
            verify(image.pixel(30, 10) == Qt.rgba(0, 0, 1, 1));

            // Column of red, green, blue and white box inside blue
            // At 10,30, spanning 10x10 pixels each
            verify(image.pixel(10, 30) == Qt.rgba(1, 0, 0, 1));
            verify(image.pixel(10, 40) == Qt.rgba(0, 1, 0, 1));
            verify(image.pixel(10, 50) == Qt.rgba(0, 0, 1, 1));

            // Flow of red, green, blue and white box inside blue
            // At 30,30, spanning 10x10 pixels each, wrapping after two boxes
            verify(image.pixel(30, 30) == Qt.rgba(1, 0, 0, 1));
            verify(image.pixel(40, 30) == Qt.rgba(0, 1, 0, 1));
            verify(image.pixel(30, 40) == Qt.rgba(0, 0, 1, 1));

            // Flow of red, green, blue and white box inside blue
            // At 100,10, spanning 10x10 pixels each, wrapping after two boxes
            verify(image.pixel(60, 10) == Qt.rgba(1, 0, 0, 1));
            verify(image.pixel(70, 10) == Qt.rgba(0, 1, 0, 1));
            verify(image.pixel(60, 20) == Qt.rgba(0, 0, 1, 1));
        }
    }

    Component {
        id: greenPassThrough
        ShaderEffect {
            fragmentShader:
            "
                uniform lowp sampler2D source;
                varying highp vec2 qt_TexCoord0;
                void main() {
                    gl_FragColor = texture2D(source, qt_TexCoord0) * vec4(0, 1, 0, 1);
                }
            "
        }
    }

    Row {
        id: theRow
        x: 10
        y: 10
        Rectangle {
            width: 10
            height: 10
            color: "#ff0000"
            layer.enabled: true
        }

        Rectangle {
            width: 10
            height: 10
            color: "#ffffff"
            layer.enabled: true
            layer.effect: greenPassThrough
        }

        Rectangle {
            id: blueInRow
            width: 10
            height: 10
            color: "#0000ff"
        }
    }

    Column {
        id: theColumn
        x: 10
        y: 30
        Rectangle {
            width: 10
            height: 10
            color: "#ff0000"
            layer.enabled: true
        }

        Rectangle {
            width: 10
            height: 10
            color: "#ffffff"
            layer.enabled: true
            layer.effect: greenPassThrough
        }

        Rectangle {
            id: blueInColumn
            width: 10
            height: 10
            color: "#0000ff"
        }
    }

    Flow {
        id: theFlow
        x: 30
        y: 30
        width: 20
        Rectangle {
            width: 10
            height: 10
            color: "#ff0000"
            layer.enabled: true
        }

        Rectangle {
            width: 10
            height: 10
            color: "#ffffff"
            layer.enabled: true
            layer.effect: greenPassThrough
        }

        Rectangle {
            id: blueInFlow
            width: 10
            height: 10
            color: "#0000ff"
        }
    }

    Grid {
        id: theGrid
        x: 60
        y: 10
        columns: 2
        Rectangle {
            width: 10
            height: 10
            color: "#ff0000"
            layer.enabled: true
        }

        Rectangle {
            width: 10
            height: 10
            color: "#ffffff"
            layer.enabled: true
            layer.effect: greenPassThrough
        }

        Rectangle {
            id: blueInGrid
            width: 10
            height: 10
            color: "#0000ff"
        }
    }

}
