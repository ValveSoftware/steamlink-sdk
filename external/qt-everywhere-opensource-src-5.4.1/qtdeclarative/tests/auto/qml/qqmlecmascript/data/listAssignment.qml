import QtQuick 2.0
import Qt.test 1.0

Item {
    id: root
    width: 640
    height: 480

    property int list1length: list1.length

    property list<MyQmlObject> list1
    property list<MyQmlObject> list2: [
        MyQmlObject { id: one; value: 100 },
        MyQmlObject { id: two; value: 300 }
    ]

    Component.onCompleted: {
        root.list1 = root.list2;
    }
}
