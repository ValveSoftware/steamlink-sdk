/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.2
import QtTest 1.0
import QtQuickControlsTests 1.0
import QtQuick.Window 2.1

Item {
    id: container
    width: 400
    height: 400

TestCase {
    id: testCase
    name: "Tests_ApplicationWindow"
    when:windowShown
    width:400
    height:400

    function test_minimumHeight() {
        var test_control = 'import QtQuick 2.2; \
        import QtQuick.Controls 1.2;            \
        ApplicationWindow {                     \
            visible: true;                      \
            width: 100; height: 100;            \
            property alias contentArea: rect;   \
            statusBar: StatusBar {              \
                visible: false;                 \
                Label {                         \
                    text: "Ready";              \
                }                               \
            }                                   \
                                                \
            toolBar: ToolBar {                  \
                visible: false;                 \
                ToolButton {                    \
                    text: "One";                \
                }                               \
            }                                   \
            Rectangle {                         \
                id: rect;                       \
                anchors.fill: parent;           \
            }                                   \
        }                                       '

        var window = Qt.createQmlObject(test_control, container, '')
        var contentArea = window.contentArea
        waitForRendering(contentArea)
        var oldHeight = contentArea.height
        compare(contentArea.height, 100)
        window.statusBar.visible = true
        waitForRendering(window.statusBar)
        verify(contentArea.height < oldHeight)

        oldHeight = contentArea.height;
        window.toolBar.visible = true
        waitForRendering(window.toolBar)
        verify(contentArea.height < oldHeight)
        window.destroy()
    }


    function test_defaultContentItemConstraints_data() {
        return [
                { tag: "height",
                        input: {height: 100},
                        expected: {implicitHeight: 100} },
                { tag: "height_y",
                        input: {height: 100, y: 10},
                        expected: {implicitHeight: 110} },
                { tag: "height_implicitHeight_anchorsFill",
                        input: {height: 100, implicitHeight: 10, anchorsFill: true},
                        expected: {implicitHeight: 10} },
                { tag: "height_implicitHeight_anchorsFill_margins",
                        input: {height: 100, implicitHeight: 10, anchorsFill: true, anchors_margins: 20},
                        expected: {implicitHeight: 50} },
                { tag: "height_anchorsFill_margins",
                        input: {height: 100, anchorsFill: true, anchors_margins: 20},
                        expected: {implicitHeight: 40} },
                { tag: "anchorsFill_margins",       //Rectangle { anchors.fill: parent; anchors.margins: 20 }
                        input: {anchorsFill: true, anchors_margins: 20},
                        expected: {implicitHeight: 40} },
                { tag: "anchorsFill_margins0",       //Rectangle { anchors.fill: parent; anchors.margins: 0 }
                        input: {anchorsFill: true, anchors_margins: 0},
                        expected: {implicitHeight: 0} },
                { tag: "minimum_implicit_maximum_anchorsFill",
                        input: {anchorsFill: true, Layout_minimumHeight: 10, implicitHeight: 100, Layout_maximumHeight: 150},
                        expected: {minimumHeight: 10, implicitHeight: 100, maximumHeight: Number.POSITIVE_INFINITY} },
                { tag: "minimum_implicit_maximum_anchorsFill_margins",
                        input: {anchorsFill: true, anchors_margins: 20, Layout_minimumHeight: 10, implicitHeight: 100, Layout_maximumHeight: 150},
                        expected: {minimumHeight: 50, implicitHeight: 140, maximumHeight: Number.POSITIVE_INFINITY} },
                { tag: "minimum_height_maximum_anchorsFill",
                        input: {anchorsFill: true, Layout_minimumHeight: 0, height: 100, Layout_maximumHeight: 150},
                        expected: {minimumHeight: 0, implicitHeight: 0, maximumHeight: Number.POSITIVE_INFINITY} },
               ];
    }
    function test_defaultContentItemConstraints(data) {
        var input = data.input
        var expected = data.expected

        var str = ''
        // serialize....
        for (var varName in input) {
            var realName = varName.replace('_', '.')
            // anchorsFill is special...
            if (realName === 'anchorsFill') {
                str += 'anchors.fill:parent;'
            } else if (input[varName] !== undefined) {
                // serialize the other properties...
                str += realName + ':' + input[varName] +';'
            }
        }

        var test_control = 'import QtQuick 2.2; \
        import QtQuick.Controls 1.2;            \
        import QtQuick.Layouts 1.1;             \
        ApplicationWindow {                     \
            id: window;                         \
            Rectangle {                         \
                id: rect;                       \
                color: \'red\';                 \
                ' + str + '\
            }                                   \
        }                                       '

        var window = Qt.createQmlObject(test_control, container, '')
        wait(0)

        for (var propName in expected) {
            compare(window.contentItem[propName], expected[propName])
        }
    }

    function test_minimumSizeLargerThan_MaximumSize() {
        var test_control = 'import QtQuick 2.2; \
        import QtQuick.Controls 1.2;            \
        import QtQuick.Layouts 1.1;             \
        ApplicationWindow {                     \
            minimumWidth: 200;                  \
            maximumWidth: 200;                  \
            minimumHeight: 200;                 \
            maximumHeight: 200;                 \
            Rectangle {                         \
                implicitWidth: 1;               \
                implicitHeight: 20;             \
            }                                   \
        }                                       '

        var window = Qt.createQmlObject(test_control, container, '')
        window.visible = true
        wait(0)
        // The following two calls will set the min,max range to be invalid
        // this should *not* produce a warning
        compare(window.height, 200)
        window.maximumHeight -= 10
        window.minimumHeight += 10
        // Restore min,max range back to sane values
        window.maximumHeight += 20
        compare(window.height, 210)

        // Do the same test for width
        compare(window.width, 200)
        window.maximumWidth-= 10
        window.minimumWidth+= 10
        // Restore back to sane values
        window.maximumWidth += 20
        compare(window.width, 210)

        window.destroy()
    }

    function test_defaultSizeHints() {
        var test_control = 'import QtQuick 2.2; \
        import QtQuick.Controls 1.2;            \
        import QtQuick.Layouts 1.1;             \
        ApplicationWindow {                     \
            Rectangle {                         \
                anchors.fill: parent;           \
                Layout.minimumWidth: 250;       \
                Layout.minimumHeight: 250;      \
                implicitWidth: 300;             \
                implicitHeight: 300;            \
                Layout.maximumWidth: 350;       \
                Layout.maximumHeight: 350;      \
            }                                   \
        }                                       '

        var window = Qt.createQmlObject(test_control, container, '')
        window.visible = true
        waitForRendering(window.contentItem)
        compare(window.minimumWidth, 250)
        compare(window.minimumHeight, 250)
        compare(window.width, 300)
        compare(window.height, 300)
        var maxLimit = Math.pow(2,24)-1
        compare(window.maximumWidth, maxLimit)
        compare(window.maximumHeight, maxLimit)
        window.destroy()
    }

    function test_invisibleContentItemChildren() {
        var test_control = 'import QtQuick 2.2; \
        import QtQuick.Controls 1.2;            \
        ApplicationWindow {                     \
            Rectangle {                         \
                visible: false;                 \
                implicitWidth: 400;             \
                implicitHeight: 400;            \
            }                                   \
            Rectangle {                         \
                anchors.fill: parent;           \
                implicitWidth: 300;             \
                implicitHeight: 300;            \
            }                                   \
        }                                       '

        var window = Qt.createQmlObject(test_control, container, '')
        window.visible = true
        waitForRendering(window.contentItem)
        compare(window.width, 300)
        compare(window.height, 300)
        window.destroy()
    }

    function test_windowHeight() {
        var test_control = 'import QtQuick 2.2; \
        import QtQuick.Controls 1.2;            \
        ApplicationWindow {                     \
            toolBar: ToolBar {                  \
                ToolButton {                    \
                    text: "Menu"                \
                }                               \
            }                                   \
            statusBar: StatusBar {              \
                Row {                           \
                    Label {                     \
                        text: "Window test"     \
                    }                           \
                }                               \
            }                                   \
                                                \
            Rectangle {                         \
                anchors.fill: parent;           \
                implicitWidth: 300;             \
                implicitHeight: 300;            \
            }                                   \
        }                                       '

        var window = Qt.createQmlObject(test_control, container, '')
        window.visible = true
        waitForRendering(window.contentItem)
        compare(window.contentItem.implicitHeight, 300)
        verify(window.height > 300)
        window.destroy()
    }

    function test_windowHeight2() {
        var test_control = 'import QtQuick 2.2; \
        import QtQuick 2.2;                     \
        import QtQuick.Controls 1.2;            \
        ApplicationWindow {                     \
            Rectangle {                         \
                anchors.fill: parent;           \
                color: "red"                    \
            }                                   \
                                                \
            menuBar: MenuBar {                  \
                Menu {                          \
                    title: qsTr("Menu")         \
                }                               \
            }                                   \
        }                                       '

        var window = Qt.createQmlObject(test_control, container, '')
        window.visible = true
        waitForRendering(window.contentItem)
        verify(window.height > 0)
        window.destroy()
    }

}
}
