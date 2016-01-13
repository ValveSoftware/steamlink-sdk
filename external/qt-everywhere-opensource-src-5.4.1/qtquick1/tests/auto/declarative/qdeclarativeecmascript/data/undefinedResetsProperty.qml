import Qt.test 1.0

MyQmlObject {
    property bool setUndefined: false

    resettableProperty: setUndefined?undefined:92
}
