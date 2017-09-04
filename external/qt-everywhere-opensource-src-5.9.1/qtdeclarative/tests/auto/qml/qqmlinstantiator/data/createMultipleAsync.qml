import QtQml 2.1

Instantiator {
    model: 10
    asynchronous: true
    delegate: QtObject {
        property bool success: true
        property int idx: index
    }
}
