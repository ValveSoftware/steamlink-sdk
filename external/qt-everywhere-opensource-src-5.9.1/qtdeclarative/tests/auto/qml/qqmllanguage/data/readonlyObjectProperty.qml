import QtQml 2.0

QtObject {
    readonly property QtObject subObject: QtObject {
        property int readWrite: 42
    }
}
