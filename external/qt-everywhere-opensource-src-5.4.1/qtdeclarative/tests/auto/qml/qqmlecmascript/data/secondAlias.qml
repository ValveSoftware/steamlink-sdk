import QtQuick 2.0

QtObject {
    id: root

    property int prop1: 100
    property int prop2: 100

    property alias alias1: root.prop1
    property alias alias2: root.prop2

    property int test: root.alias2

    Component.onCompleted: root.prop2 = 200
}
