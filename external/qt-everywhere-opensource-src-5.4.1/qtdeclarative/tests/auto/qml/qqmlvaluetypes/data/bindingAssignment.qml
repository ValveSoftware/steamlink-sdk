import QtQuick 2.0
import Test 1.0

MyTypeObject {
    property int value: 10

    rect.x: value

    Component.onCompleted: {
        rect.y = Qt.binding(function() { return value + 5; });
    }
}
