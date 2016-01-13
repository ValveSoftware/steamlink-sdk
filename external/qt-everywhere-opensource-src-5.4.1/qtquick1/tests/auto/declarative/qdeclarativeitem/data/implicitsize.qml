import QtQuick 1.1

Item {
    implicitWidth: 200
    implicitHeight: 100

    width: 80
    height: 60

    function resetSize() {
        width = undefined
        height = undefined
    }

    function changeImplicit() {
        implicitWidth = 150
        implicitHeight = 80
    }
}
