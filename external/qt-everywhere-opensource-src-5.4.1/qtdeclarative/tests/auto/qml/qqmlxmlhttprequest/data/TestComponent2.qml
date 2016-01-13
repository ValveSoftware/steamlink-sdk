import QtQuick 2.0

Item {
    id: root
    property int a: 5
    Component.onCompleted: root.parent.finished()
}
