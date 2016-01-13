import QtQuick 2.0
import Test 1.0

MyTypeObject {
    property int value: 10
    rect.y: Qt.binding(function() { return value; }); // error.

    Component.onCompleted: {
        rect.x = 5;
        rect.x = (function() { return value; }); // error.
    }
}
