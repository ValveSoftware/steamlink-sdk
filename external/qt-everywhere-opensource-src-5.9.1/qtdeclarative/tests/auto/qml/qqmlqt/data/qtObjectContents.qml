import QtQuick 2.0

QtObject {
    property var values: Object()
    Component.onCompleted: {
        for (var key in Qt) {
            values[key] = Qt[key]
        }
    }
}
