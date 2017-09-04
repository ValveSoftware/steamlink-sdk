import QtQuick 2.0

QtObject {
    property QtObject o1
    property QtObject o2

    property alias a: object2.a

    o1: QtObject { id: object1 }
    o2: QtObject {
        id: object2
        property int a: 1923
    }
}
