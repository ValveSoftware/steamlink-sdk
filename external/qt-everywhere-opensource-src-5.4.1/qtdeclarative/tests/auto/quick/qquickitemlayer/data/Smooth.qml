import QtQuick 2.0


Item {
    width: 200
    height: 100

    Row {
        id: layerRoot

        width: 20
        height: 10

        Rectangle { width: 10; height: 10; color: "red" }
        Rectangle { width: 10; height: 10; color: "blue" }

        layer.enabled: true
        layer.smooth: true

        anchors.centerIn: parent
        scale: 10
   }
}
