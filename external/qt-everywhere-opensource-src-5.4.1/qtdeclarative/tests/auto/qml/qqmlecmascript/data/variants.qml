import QtQuick 2.2

ListView
{
    property variant undefinedVariant: undefined
    property variant nullVariant: null
    property variant intVariant: 1
    property variant doubleVariant: 1.2

    property var testVar
    property variant testVariant

    function checkNull() {
        var result = [{'test': null}];
        model = result;
        if (model[0].test !== null)
            return false;
        testVar = null;
        testVariant = testVar;
        if (testVariant !== null)
            return false;
        testVar = testVariant;
        if (testVar !== null)
            return false;
        return true;
    }
    function checkUndefined() {
        var result = [{'test': undefined}];
        model = result;
        if (model[0].test !== undefined)
            return false;
        testVar = undefined;
        testVariant = testVar;
        if (testVariant !== undefined)
            return false;
        testVar = testVariant;
        if (testVar !== undefined)
            return false;
        return true;
    }
    function checkNumber() {
        var result = [{'test': 1}];
        model = result;
        if (model[0].test !== 1)
            return false;
        testVar = 1;
        testVariant = testVar;
        if (testVariant !== 1)
            return false;
        testVar = testVariant;
        if (testVar !== 1)
            return false;
        return true;
    }
}
