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
import QtPositioning 5.2
import "utils.js" as Utils

TestCase {
    id: testCase

    name: "PlaceSearchSuggestionModel"

    PlaceSearchSuggestionModel {
        id: testModel
    }

    PlaceSearchSuggestionModel {
        id: testModelError
    }

    Plugin {
        id: testPlugin
        name: "qmlgeo.test.plugin"
        allowExperimental: true
    }

    Plugin {
        id: nonExistantPlugin
        name: "nonExistantName"
    }

    Plugin {
        id: uninitializedPlugin
    }

    function test_setAndGet_data() {
        var testSearchArea = QtPositioning.circle(QtPositioning.coordinate(10, 20), 5000);
        return [
            { tag: "plugin", property: "plugin", signal: "pluginChanged", value: testPlugin },
            { tag: "searchArea", property: "searchArea", signal: "searchAreaChanged", value: testSearchArea, reset: QtPositioning.shape() },
            { tag: "limit", property: "limit", signal: "limitChanged", value: 10, reset: -1 },

            { tag: "searchTerm", property: "searchTerm", signal: "searchTermChanged", value: "Test term", reset: "" },
        ];
    }

    function test_setAndGet(data) {
        Utils.testObjectProperties(testCase, testModel, data);
    }

    SignalSpy { id: statusChangedSpy; target: testModel; signalName: "statusChanged" }
    SignalSpy { id: suggestionsChangedSpy; target: testModel; signalName: "suggestionsChanged" }

    function test_suggestions() {
        compare(statusChangedSpy.count, 0);
        testModel.plugin = testPlugin;

        compare(testModel.status, PlaceSearchSuggestionModel.Null);

        testModel.searchTerm = "test";
        testModel.update();

        compare(testModel.status, PlaceSearchSuggestionModel.Loading);
        compare(statusChangedSpy.count, 1);

        tryCompare(testModel, "status", PlaceSearchSuggestionModel.Ready);
        compare(statusChangedSpy.count, 2);

        var expectedSuggestions = [ "test1", "test2", "test3" ];

        compare(suggestionsChangedSpy.count, 1);
        compare(testModel.suggestions, expectedSuggestions);

        testModel.reset();

        compare(statusChangedSpy.count, 3);
        compare(testModel.status, PlaceSearchSuggestionModel.Null);
        compare(suggestionsChangedSpy.count, 2);
        compare(testModel.suggestions, []);

        testModel.update();

        compare(statusChangedSpy.count, 4);
        compare(testModel.status, PlaceSearchSuggestionModel.Loading);

        testModel.cancel();

        compare(statusChangedSpy.count, 5);
        compare(testModel.status, PlaceSearchSuggestionModel.Ready);

        //check that an encountering an error will cause the model
        //to clear its data
        testModel.plugin = null;
        testModel.update();
        tryCompare(testModel.suggestions, "length", 0);
        compare(testModel.status, PlaceSearchSuggestionModel.Error);
    }

    SignalSpy { id: statusChangedSpyError; target: testModelError; signalName: "statusChanged" }

    function test_error() {
        compare(statusChangedSpyError.count, 0);
        //try searching without a plugin instance
        testModelError.update();
        tryCompare(statusChangedSpyError, "count", 2);
        compare(testModelError.status, PlaceSearchSuggestionModel.Error);
        statusChangedSpyError.clear();
        //Aside: there is some difficulty in checking the transition to the Loading state
        //since the model transitions from Loading to Error before the next event loop
        //iteration.

        //try searching with an uninitialized plugin instance.
        testModelError.plugin = uninitializedPlugin;
        testModelError.update();
        tryCompare(statusChangedSpyError, "count", 2);
        compare(testModelError.status, PlaceSearchSuggestionModel.Error);
        statusChangedSpyError.clear();

        //try searching with plugin a instance
        //that has been provided a non-existent name
        testModelError.plugin = nonExistantPlugin;
        testModelError.update();
        tryCompare(statusChangedSpyError, "count", 2);
        compare(testModelError.status, PlaceSearchSuggestionModel.Error);
    }
}
