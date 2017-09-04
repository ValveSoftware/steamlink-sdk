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

import QtQuick 2.0
import QtTest 1.0
import QtLocation 5.3
import "utils.js" as Utils

TestCase {
    id: testCase

    name: "CategoryModel"

    CategoryModel {
        id: testModel
    }

    Plugin {
        id: testPlugin
        name: "qmlgeo.test.plugin"
        allowExperimental: true
        parameters: [
            PluginParameter {
                name: "initializePlaceData"
                value: true
            }
        ]
    }

    Plugin {
        id: uninitializedPlugin
    }

    Plugin {
        id: nonExistantPlugin
        name: "nonExistentName"
    }

    function test_setAndGet_data() {
        return [
            { tag: "plugin", property: "plugin", signal: "pluginChanged", value: testPlugin },
            { tag: "hierarchical", property: "hierarchical", signal: "hierarchicalChanged", value: false, reset: true },
        ];
    }

    function test_setAndGet(data) {
        Utils.testObjectProperties(testCase, testModel, data);
    }

    function test_hierarchicalModel() {
        var modelSpy = Qt.createQmlObject('import QtTest 1.0; SignalSpy {}', testCase, "SignalSpy");
        var categoryModel = Qt.createQmlObject('import QtQuick 2.0; import QtLocation 5.3;'
                                + 'VisualDataModel { model: CategoryModel {} delegate: Item {} }',
                                testCase, "VisualDataModel");

        modelSpy.target = categoryModel.model;
        modelSpy.signalName = "statusChanged";

        compare(categoryModel.model.status, CategoryModel.Null);
        compare(categoryModel.count, 0);


        // set the plugin
        categoryModel.model.plugin = testPlugin;
        categoryModel.model.update();
        tryCompare(categoryModel.model, "status", CategoryModel.Loading);
        compare(modelSpy.count, 1);

        tryCompare(categoryModel.model, "status", CategoryModel.Ready);
        compare(modelSpy.count, 2);
        compare(categoryModel.model.errorString(), "");

        var expectedNames = [ "Accommodation", "Park" ];

        compare(categoryModel.count, expectedNames.length);

        for (var i = 0; i < expectedNames.length; ++i) {
            var category = categoryModel.model.data(categoryModel.modelIndex(i),
                                                    CategoryModel.CategoryRole);
            compare(category.name, expectedNames[i]);
        }


        // check that "Accommodation" has children
        categoryModel.rootIndex = categoryModel.modelIndex(0);

        expectedNames = [ "Camping", "Hotel", "Motel" ];

        compare(categoryModel.count, expectedNames.length);

        for (i = 0; i < expectedNames.length; ++i) {
            category = categoryModel.model.data(categoryModel.modelIndex(i),
                                                    CategoryModel.CategoryRole);
            compare(category.name, expectedNames[i]);

            var parentCategory = categoryModel.model.data(categoryModel.modelIndex(i),
                                                         CategoryModel.ParentCategoryRole);
            compare(parentCategory.name, "Accommodation");
        }

        categoryModel.rootIndex = categoryModel.parentModelIndex();

        compare(categoryModel.count, 2);


        // check that "Park" has no children
        categoryModel.rootIndex = categoryModel.modelIndex(1);

        compare(categoryModel.count, 0);

        categoryModel.rootIndex = categoryModel.parentModelIndex();


        // clean up
        categoryModel.model.plugin = null;
        categoryModel.model.update();

        // check that the model is empty when an error is encountered
        tryCompare(categoryModel, "count", 0);
        compare(categoryModel.model.status, CategoryModel.Error);
    }

    function test_flatModel() {
        var modelSpy = Qt.createQmlObject('import QtTest 1.0; SignalSpy {}', testCase, "SignalSpy");
        var categoryModel = Qt.createQmlObject('import QtQuick 2.0; import QtLocation 5.3;'
                                + 'VisualDataModel { model: CategoryModel {} delegate: Item {} }',
                                testCase, "VisualDataModel");

        modelSpy.target = categoryModel.model;
        modelSpy.signalName = "statusChanged";

        compare(categoryModel.model.status, CategoryModel.Null);
        compare(categoryModel.count, 0);


        // set the plugin
        categoryModel.model.hierarchical = false;
        categoryModel.model.plugin = testPlugin;

        categoryModel.model.update();
        tryCompare(categoryModel.model, "status", CategoryModel.Loading);
        compare(modelSpy.count, 1);

        tryCompare(categoryModel.model, "status", CategoryModel.Ready);
        compare(modelSpy.count, 2);

        var expectedNames = [ "Accommodation", "Camping", "Hotel", "Motel", "Park" ];

        compare(categoryModel.count, expectedNames.length);

        for (var i = 0; i < expectedNames.length; ++i) {
            var category = categoryModel.model.data(categoryModel.modelIndex(i),
                                                    CategoryModel.CategoryRole);
            var name = categoryModel.model.data(categoryModel.modelIndex(i), 0);    // DisplayRole

            compare(name, expectedNames[i]);
            compare(category.name, expectedNames[i]);
        }


        // check that no category has children
        for (i = 0; i < categoryModel.count; ++i) {
            categoryModel.rootIndex = categoryModel.modelIndex(i);

            compare(categoryModel.count, 0);

            categoryModel.rootIndex = categoryModel.parentModelIndex();
        }


        // clean up
        categoryModel.model.hierarchical = true;
        categoryModel.model.plugin = null;


        // check that the model is empty when an error is encountered
        categoryModel.model.update();
        tryCompare(categoryModel, "count", 0);
        compare(categoryModel.model.status, CategoryModel.Error);
    }

    function test_error() {
        var testModel = Qt.createQmlObject('import QtLocation 5.3; CategoryModel {}', testCase, "CategoryModel");

        var statusChangedSpy = Qt.createQmlObject('import QtTest 1.0; SignalSpy {}', testCase, "SignalSpy");
        statusChangedSpy.target = testModel;
        statusChangedSpy.signalName = "statusChanged";

        //try updating without a plugin instance
        testModel.update();
        tryCompare(statusChangedSpy, "count", 2);
        compare(testModel.status, CategoryModel.Error);
        statusChangedSpy.clear();
        //Aside: there is some difficulty in checking the transition to the Loading state
        //since the model transitions from Loading to Error before the next event loop
        //iteration.

        //try updating with an uninitialized plugin instance.
        testModel.plugin = uninitializedPlugin;
        testModel.update();
        tryCompare(statusChangedSpy, "count", 2);
        compare(testModel.status, CategoryModel.Error);
        statusChangedSpy.clear();

        //try searching with plugin a instance
        //that has been provided a non-existent name
        testModel.plugin = nonExistantPlugin;
        testModel.update();
        tryCompare(statusChangedSpy, "count", 2);
        compare(testModel.status, CategoryModel.Error);
    }
}
