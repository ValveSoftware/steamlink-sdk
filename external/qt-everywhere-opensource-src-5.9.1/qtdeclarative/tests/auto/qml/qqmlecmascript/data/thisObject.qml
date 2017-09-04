import QtQml 2.0
QtObject {
    property int value: 1
    property QtObject subObject: QtObject {
        property int value: 2
        property int test: {
            return this.value;
        }
    }
}
