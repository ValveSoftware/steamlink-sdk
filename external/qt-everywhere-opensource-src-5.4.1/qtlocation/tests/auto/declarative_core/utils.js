.pragma library

function compareArray(a, b) {
    if (a.length !== b.length)
        return false;

    for (var i = 0; i < a.length; ++i) {
        var aMatched = false;
        var bMatched = false;

        for (var j = 0; j < b.length; ++j) {
            if (a[i] === b[j])
                aMatched = true;
            if (b[i] === a[j])
                bMatched = true;
            if (aMatched && bMatched)
                break;
        }

        if (!aMatched || !bMatched)
            return false;
    }

    return true;
}

function testObjectProperties(testCase, testObject, data) {
    var signalSpy = Qt.createQmlObject('import QtTest 1.0; SignalSpy {}', testCase, "SignalSpy");
    signalSpy.target = testObject;
    signalSpy.signalName = data.signal;

    // set property to something new
    testObject[data.property] = data.value;
    if (data.array) {
        if (data.expectedValue) {
            testCase.verify(compareArray(testObject[data.property], data.expectedValue));
            testCase.compare(signalSpy.count, 1 + data.expectedValue.length);
        } else {
            testCase.verify(compareArray(testObject[data.property], data.value));
            testCase.compare(signalSpy.count, 1 + data.value.length);
        }

    } else {
        testCase.compare(testObject[data.property], data.value);
        testCase.compare(signalSpy.count, 1);
    }

    signalSpy.clear();

    // set property to same value
    testObject[data.property] = data.value;
    if (data.array) {
        if (data.expectedValue) {
            testCase.verify(compareArray(testObject[data.property], data.expectedValue));
            testCase.compare(signalSpy.count, 1 + data.expectedValue.length);
        } else {
            testCase.verify(compareArray(testObject[data.property], data.value));
            testCase.compare(signalSpy.count, 1 + data.value.length);
        }

    } else {
        testCase.compare(testObject[data.property], data.value);
        testCase.compare(signalSpy.count, 0);
    }

    signalSpy.clear();

    // reset property
    if (data.reset === undefined) {
        testObject[data.property] = null;
        testCase.compare(testObject[data.property], null);
    } else {
        testObject[data.property] = data.reset;
        if (data.array)
            testCase.verify(compareArray(testObject[data.property], data.reset));
        else
            testCase.compare(testObject[data.property], data.reset);
    }
    testCase.compare(signalSpy.count, 1);
    signalSpy.destroy();
}

function compareObj(testCase, obj1, obj2) {
    for (var propertyName in obj2) {
        if (obj1[propertyName] !== undefined) {
            if (propertyName === "dateTime" && isNaN(obj2["dateTime"].getTime()))
                testCase.verify(isNaN(obj1["dateTime"].getTime()));
            else
                testCase.compare(obj1[propertyName], obj2[propertyName])
        }
    }
}

function testConsecutiveFetch(testCase, model, place, expectedValues, data)
{
    var signalSpy = Qt.createQmlObject('import QtTest 1.0; SignalSpy {}', testCase, "SignalSpy");
    signalSpy.target = model;
    signalSpy.signalName ="totalCountChanged";

    var visDataModel = Qt.createQmlObject('import QtQuick 2.0; '
                                       + 'VisualDataModel{ delegate: Text{} }',
                                       testCase, "dataModel");
    visDataModel.model = model;

    //check that initial values are as expected
    testCase.compare(model.totalCount, -1);
    testCase.compare(model.place, null);
    testCase.compare(visDataModel.items.count, 0);

    //perform an initial fetch with the default batch size
    model.batchSize = data.batchSize
    model.place = place;
    testCase.tryCompare(signalSpy, "count", 1);
    signalSpy.clear();

    var totalCount = model.totalCount;
    testCase.compare(totalCount, 5);
    testCase.compare(visDataModel.items.count, Math.min(data.batchSize, totalCount));

    compareObj(testCase, visDataModel.items.get(0).model, expectedValues[0]);

    //fetch remaining items, in batchSize batches
    while (visDataModel.items.count < totalCount) {
        var startIndex = visDataModel.items.count

        //'creating' the last item will trigger a fetch
        visDataModel.items.create(visDataModel.items.count - 1);

        testCase.tryCompare(visDataModel.items, "count", Math.min(totalCount, startIndex + data.batchSize));
        testCase.compare(signalSpy.count, 0);
        testCase.compare(model.totalCount, totalCount);

        for (var i = startIndex; i < Math.min(totalCount, startIndex + data.batchSize); ++i)
            compareObj(testCase, visDataModel.items.get(i).model, expectedValues[i]);
    }

    visDataModel.destroy();
    signalSpy.destroy();
}

function testReset(testCase, model, place)
{
    var dataModel = Qt.createQmlObject('import QtQuick 2.0; '
                                       + 'VisualDataModel{ delegate: Text{} }',
                                       testCase, "dataModel");

    dataModel.model = model;
    model.place = place;
    testCase.wait(1);
    testCase.verify(model.totalCount > 0);
    testCase.verify(dataModel.items.count > 0);

    model.place = null;
    testCase.tryCompare(model, "totalCount", -1);
    testCase.compare(dataModel.items.count, 0);

    dataModel.destroy();
}

function testFetch(testCase, data)
{
    var model = data.model;
    var visDataModel = Qt.createQmlObject('import QtQuick 2.0; '
                                       + 'VisualDataModel{ delegate: Text{} }',
                                       testCase, "dataModel");
    visDataModel.model = model

    var signalSpy = Qt.createQmlObject('import QtTest 1.0; SignalSpy {}',
                                       testCase, "SignalSpy");
    signalSpy.target = model;
    signalSpy.signalName ="totalCountChanged";

    model.batchSize = data.batchSize;
    model.place = data.place;
    testCase.tryCompare(signalSpy, "count", 1);
    signalSpy.clear();
    testCase.compare(model.totalCount, data.expectedTotalCount);
    testCase.compare(visDataModel.items.count, data.expectedCount);

    visDataModel.destroy();
    signalSpy.destroy();
}
