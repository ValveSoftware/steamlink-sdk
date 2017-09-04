/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

import QtQuick 2.8
import QtQuick.Window 2.2
import QtTest 1.2

TestCase {
    id: testCase
    name: "tst_createTemporaryObject"
    width: 100
    height: 100
    when: windowShown

    property var createdObjectNames: []
    property var createdParentlessObjects: []

    function verifyNoChildren() {
        for (var i = 0; i < createdObjectNames.length; ++i) {
            verify(!findChild(testCase, createdObjectNames[i]));
        }

        compare(createdParentlessObjects.length, 0,
            "The following parentless temporary objects were not destroyed: " + createdParentlessObjects)
    }

    function init() {
        // The items are destroyed after cleanup(), so we check here after every test,
        // and once for the last test in cleanupTestCase().
        verifyNoChildren();
    }

    function cleanupTestCase() {
        verifyNoChildren();
    }

    function test_fromQml_data() {
        return [
            { tag: "QtObject", qml: "import QtQml 2.0; QtObject {}" },
            { tag: "Item", qml: "import QtQuick 2.0; Item {}" },
        ];
    }

    function test_fromQml(data) {
        var object = createTemporaryQmlObject(data.qml, testCase);
        verify(object);

        object.objectName = data.tag + "FromQml";
        compare(findChild(testCase, object.objectName), object);

        createdObjectNames.push(object.objectName);

        // Create an object and destroy it early. It should be
        // removed from TestCase's list of temporary objects
        // as soon as it's destroyed.
        var manuallyDestroyedObject = createTemporaryQmlObject(data.qml, testCase);
        verify(manuallyDestroyedObject);

        var manuallyDestroyedObjectName = data.tag + "FromQmlShortLived";
        manuallyDestroyedObject.objectName = manuallyDestroyedObjectName;
        compare(findChild(testCase, manuallyDestroyedObjectName), manuallyDestroyedObject);

        manuallyDestroyedObject.destroy();
        wait(0);

        verify(!findChild(testCase, manuallyDestroyedObjectName));
    }

    Component {
        id: objectComponent

        QtObject {}
    }

    Component {
        id: itemComponent

        Item {}
    }

    Component {
        id: windowComponent

        Window {}
    }

    function test_fromComponent_data() {
        return [
            { tag: "QtObject", component: objectComponent },
            { tag: "Item", component: itemComponent },
            { tag: "Window", component: windowComponent },
        ];
    }

    function test_fromComponent(data) {
        var object = createTemporaryObject(data.component, testCase);
        verify(object);

        object.objectName = data.tag + "FromComponent";
        compare(findChild(testCase, object.objectName), object);

        if (object.hasOwnProperty("contentItem"))
            object.contentItem.objectName = "WindowContentItemFromComponent";

        createdObjectNames.push(object.objectName);

        // Create an object and destroy it early. It should be
        // removed from TestCase's list of temporary objects
        // as soon as it's destroyed.
        var manuallyDestroyedObject = createTemporaryObject(data.component, testCase);
        verify(manuallyDestroyedObject);

        var manuallyDestroyedObjectName = data.tag + "FromComponentShortLived";
        manuallyDestroyedObject.objectName = manuallyDestroyedObjectName;
        compare(findChild(testCase, manuallyDestroyedObjectName), manuallyDestroyedObject);

        manuallyDestroyedObject.destroy();
        wait(0);

        verify(!findChild(testCase, manuallyDestroyedObjectName));
    }

    function test_fromComponentParent_data() {
        return [
            { tag: "omit", expectedParent: null },
            { tag: "undefined", parent: undefined, expectedParent: null },
            { tag: "null", parent: null, expectedParent: null },
            { tag: "1", parent: 1, expectedParent: null },
            { tag: "testCase", parent: testCase, expectedParent: testCase }
        ];
    }

    // Tests that an invalid or missing parent argument results in a parentless object.
    // This is the same behavior as displayed by component.createObject().
    function test_fromComponentParent(data) {
        var object = data.hasOwnProperty("parent")
            ? createTemporaryObject(itemComponent, data.parent)
            : createTemporaryObject(itemComponent);
        verify(object);
        compare(object.parent, data.expectedParent);

        object.objectName = data.tag + "FromComponentOmitParent";
        if (object.parent) {
            compare(findChild(testCase, object.objectName), object);
            createdObjectNames.push(object.objectName);
        } else {
            object.Component.destruction.connect(function() {
                 var indexOfObject = createdParentlessObjects.indexOf(object);
                 createdParentlessObjects.splice(indexOfObject, 1);
            });
            createdParentlessObjects.push(object);
        }
    }
}
