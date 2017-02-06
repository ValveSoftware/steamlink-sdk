import QtQuick 2.0

Item {
    width: 200
    height: 200
    Rectangle {
        width: 100
        height: 100
        color: "red"
        layer.enabled: true
        Component.onCompleted: {
            layer.enabled = false
            visible = false
            width = 120
            x = 10
        }
    }
}
