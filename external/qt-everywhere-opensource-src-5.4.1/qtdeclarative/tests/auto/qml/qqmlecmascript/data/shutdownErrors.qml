import QtQuick 2.0

Item {
    property int test: myObject.object.a

    Item {
        id: myObject
        property QtObject object;
        object: QtObject {
            property int a: 10
        }
    }
}
