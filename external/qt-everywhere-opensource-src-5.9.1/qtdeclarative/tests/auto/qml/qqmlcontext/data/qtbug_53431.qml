import QtQml 2.0
QtObject {
    property int value: {
        console.log("lookup in global object")
        return 1
    }
}
