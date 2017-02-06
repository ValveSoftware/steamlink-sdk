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

    name: "PlaceSearchModel"

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
        id: favoritePlugin
        name: "foo"
    }

    Plugin {
        id: uninitializedPlugin
    }

    Category {
        id: testCategory1
        categoryId: "da3606c1-3448-43b3-a4a3-ca24b12dd94a"
        name: "Test Category 1"
    }

    Category {
        id: testCategory2
        categoryId: "bb8ead84-ec2a-48a9-9c8f-d4ffd3134b21"
        name: "Test Category 2"
    }

    function compareArray(a, b) {
        if (a.length !== b.length)
            return false;

        for (var i = 0; i < a.length; ++i) {
            if (b.indexOf(a[i]) < 0)
                return false;
        }

        return true;
    }

    function test_setAndGet_data() {
        var testSearchArea = QtPositioning.circle(QtPositioning.coordinate(10, 20), 5000);

        return [
            { tag: "plugin", property: "plugin", signal: "pluginChanged", value: testPlugin },
            { tag: "searchArea", property: "searchArea", signal: "searchAreaChanged", value: testSearchArea, reset: QtPositioning.shape() },
            { tag: "limit", property: "limit", signal: "limitChanged", value: 10, reset: -1 },

            { tag: "searchTerm", property: "searchTerm", signal: "searchTermChanged", value: "Test term", reset: "" },
            { tag: "recommendationId", property: "recommendationId", signal: "recommendationIdChanged", value: "Test-place-id", reset: "" },
            { tag: "relevanceHint", property: "relevanceHint", signal: "relevanceHintChanged", value: PlaceSearchModel.DistanceHint, reset: PlaceSearchModel.UnspecifiedHint },
            { tag: "visibilityScope", property: "visibilityScope", signal: "visibilityScopeChanged", value: Place.DeviceVisibility, reset: Place.UnspecifiedVisibility },
            { tag: "favoritesPlugin", property: "favoritesPlugin", signal: "favoritesPluginChanged", value: favoritePlugin },
            { tag: "category", property: "categories", signal: "categoriesChanged", value: testCategory1, expectedValue: [ testCategory1 ], reset: [], array: true },
            { tag: "categories", property: "categories", signal: "categoriesChanged", value: [ testCategory1, testCategory2 ], reset: [], array: true },
        ];
    }

    function test_setAndGet(data) {
        var testModel = Qt.createQmlObject('import QtLocation 5.3; PlaceSearchModel {}', testCase, "PlaceSearchModel");
        Utils.testObjectProperties(testCase, testModel, data);
        delete testModel;
    }

    function test_search_data() {
        var park = Qt.createQmlObject('import QtLocation 5.3; Category {name: "Park"; categoryId: "c2e1252c-b997-44fc-8165-e53dd00f66a7"}', testCase, "Category");
        return [
            {
                tag: "searchTerm, multiple results",
                property: "searchTerm",
                value: "view",
                reset: "",
                places: [
                    "4dcc74ce-fdeb-443e-827c-367438017cf1",
                    "8f72057a-54b2-4e95-a7bb-97b4d2b5721e"
                ]
            },
            {
                tag: "searchTerm, single result",
                property: "searchTerm",
                value: "park",
                reset: "",
                places: [
                    "4dcc74ce-fdeb-443e-827c-367438017cf1"
                ]
            },
            {
                tag: "categories, single result",
                property: "categories",
                value: [ park ],
                places: [
                    "dacb2181-3f67-4e6a-bd4d-635e99ad5b03"
                ]
            },
            {
                tag: "recommendations",
                property: "recommendationId",
                value: "4dcc74ce-fdeb-443e-827c-367438017cf1",
                reset: "",
                places: [
                    "8f72057a-54b2-4e95-a7bb-97b4d2b5721e",
                    "dacb2181-3f67-4e6a-bd4d-635e99ad5b03"
                ]
            },
            {
                tag: "no recommendations",
                property: "recommendationId",
                value: "8f72057a-54b2-4e95-a7bb-97b4d2b5721e",
                reset: "",
                places: [ ]
            }
        ];
    }

    function test_search(data) {
        var testModel = Qt.createQmlObject('import QtLocation 5.3; PlaceSearchModel {}', testCase, "PlaceSearchModel");
        testModel.plugin = testPlugin;

        var statusChangedSpy = Qt.createQmlObject('import QtTest 1.0; SignalSpy {}', testCase, "SignalSpy");
        statusChangedSpy.target = testModel;
        statusChangedSpy.signalName = "statusChanged";

        var countChangedSpy = Qt.createQmlObject('import QtTest 1.0; SignalSpy {}', testCase, "SignalSpy");
        countChangedSpy.target = testModel;
        countChangedSpy.signalName = "rowCountChanged";

        compare(testModel.status, PlaceSearchModel.Null);

        testModel[data.property] = data.value;
        testModel.update();

        compare(testModel.status, PlaceSearchModel.Loading);
        compare(statusChangedSpy.count, 1);

        tryCompare(testModel, "status", PlaceSearchModel.Ready);
        compare(statusChangedSpy.count, 2);

        if (data.places.length > 0)
            compare(countChangedSpy.count, 1);
        else
            compare(countChangedSpy.count, 0);

        for (var i = 0; i < testModel.count; ++i) {
            compare(testModel.data(i, "type"), PlaceSearchModel.PlaceResult);

            var place = testModel.data(i, "place");

            verify(data.places.indexOf(place.placeId) >= 0);
        }

        testModel.reset();

        compare(statusChangedSpy.count, 3);
        compare(testModel.status, PlaceSearchModel.Null);
        if (data.places.length > 0)
            compare(countChangedSpy.count, 2);
        else
            compare(countChangedSpy.count, 0);
        compare(testModel.count, 0);

        countChangedSpy.destroy();
        statusChangedSpy.destroy();

        if (data.reset === undefined) {
            testModel[data.property] = null;
        } else {
            testModel[data.property] = data.reset;
        }

        delete testModel;
        delete statusChangedSpy;
        delete countChangedSpy;
    }

    function test_cancel() {
        var testModel = Qt.createQmlObject('import QtLocation 5.3; PlaceSearchModel {}', testCase, "PlaceSearchModel");
        testModel.plugin = testPlugin;

        var statusChangedSpy = Qt.createQmlObject('import QtTest 1.0; SignalSpy {}', testCase, "SignalSpy");
        statusChangedSpy.target = testModel;
        statusChangedSpy.signalName = "statusChanged";

        //try cancelling from an initially null state
        compare(testModel.status, PlaceSearchModel.Null);
        testModel.searchTerm = "view";
        testModel.update();
        tryCompare(testModel, "status", PlaceSearchModel.Loading);
        testModel.cancel();
        tryCompare(testModel, "status", PlaceSearchModel.Ready);
        compare(statusChangedSpy.count, 2);

        testModel.update();
        tryCompare(testModel, "status", PlaceSearchModel.Loading);
        tryCompare(testModel, "status", PlaceSearchModel.Ready);
        compare(statusChangedSpy.count, 4);

        var numResults = testModel.count;
        verify(numResults > 0);

        //try cancelling from an initially ready state
        testModel.update();
        tryCompare(testModel, "status", PlaceSearchModel.Loading);
        testModel.cancel();
        tryCompare(testModel, "status", PlaceSearchModel.Ready);
        compare(testModel.count, numResults);
        compare(statusChangedSpy.count, 6);

        //chack that an encountering an error will cause the model
        //to clear its data
        testModel.plugin = null;
        testModel.update();
        tryCompare(testModel, "count", 0);
        compare(testModel.status, PlaceSearchModel.Error);

        delete testModel;
        delete statusChangedSpy;
    }

    function test_error() {
        var testModel = Qt.createQmlObject('import QtLocation 5.3; PlaceSearchModel {}', testCase, "PlaceSearchModel");

        var statusChangedSpy = Qt.createQmlObject('import QtTest 1.0; SignalSpy {}', testCase, "SignalSpy");
        statusChangedSpy.target = testModel;
        statusChangedSpy.signalName = "statusChanged";

        //try searching without a plugin instance
        testModel.update();
        tryCompare(statusChangedSpy, "count", 2);
        compare(testModel.status, PlaceSearchModel.Error);
        statusChangedSpy.clear();
        //Aside: there is some difficulty in checking the transition to the Loading state
        //since the model transitions from Loading to Error before the next event loop
        //iteration.

        //try searching with an uninitialized plugin instance.
        testModel.plugin = uninitializedPlugin;
        testModel.update();
        tryCompare(statusChangedSpy, "count", 2);
        compare(testModel.status, PlaceSearchModel.Error);
        statusChangedSpy.clear();

        //try searching with plugin a instance
        //that has been provided a non-existent name
        testModel.plugin = favoritePlugin;
        testModel.update();
        tryCompare(statusChangedSpy, "count", 2);
        compare(testModel.status, PlaceSearchModel.Error);
    }
}
