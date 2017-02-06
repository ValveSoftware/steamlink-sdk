import Test 1.0
import QtQuick 2.0

QtObject {
    property alias obj : otherObj
    property variant child
    child: QtObject {
        id: otherObj
        property int myValue: 10
    }
}

