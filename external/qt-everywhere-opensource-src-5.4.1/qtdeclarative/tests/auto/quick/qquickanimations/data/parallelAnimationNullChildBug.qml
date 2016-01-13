import QtQuick 2.2

Item {
    id: root
    Component.onCompleted: Qt.createQmlObject("import QtQuick 2.2; ParallelAnimation{animations: [null]}", root)
}
