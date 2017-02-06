import QtQml 2.0
NullObjectInitializerBase {
    testProperty: null
    property bool success: false
    Component.onCompleted: {
        success = testProperty === null;
    }
}
