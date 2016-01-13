import QtQuick 1.0

QtObject {
    id: root

    property variant a: 10
    property int x: 10
    property int test: a.x

    Component.onCompleted: {
        a = 11;
        a = root;
    }
}
