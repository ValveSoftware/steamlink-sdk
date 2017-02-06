import QtQml 2.0
QtObject {
    property int value: 100
    property bool success: {
        unrelatedFunction();
        return value == 42;
    }
    property int unrelatedValue: 100
    function unrelatedFunction() {
        return unrelatedValue;
    }
}
