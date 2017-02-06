import QtQuick 2.0

Item {
    width: 200
    height: 200
    Rectangle {
        width: 100
        height: 100
        color: "red"
        Component.onCompleted: {
            layer.enabled = true
            layer.effect = effectComponent
            layer.enabled = false
            visible = false
            width = 120
            x = 10
        }
    }
    Component {
        id: effectComponent
        ShaderEffect { }
    }
}
