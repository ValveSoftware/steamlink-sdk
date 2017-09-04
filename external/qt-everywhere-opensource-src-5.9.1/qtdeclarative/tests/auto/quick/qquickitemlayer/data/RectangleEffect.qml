import QtQuick 2.0

Item {
    width: 200
    height: 200
    Rectangle {
        width: 100
        height: 100
        x: 50
        y: 50
        scale: 1.5
        z: 1
        rotation: 45
        color: "#ff0000"
        layer.enabled: true
        layer.effect: Rectangle { color: "#0000ff" }
    }
    Rectangle {
        anchors.fill: parent
        color: "#00ff00"
    }
}
