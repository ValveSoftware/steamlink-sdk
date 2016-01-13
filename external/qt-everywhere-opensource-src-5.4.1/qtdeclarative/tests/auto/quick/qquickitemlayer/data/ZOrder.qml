import QtQuick 2.0

Item
{
    id: root

    width: 200
    height: 200

    Component {
        id: shaderEffect
        ShaderEffect { }
    }

    property bool layerEffect: false;
    onLayerEffectChanged: root.maybeUse();
    Component.onCompleted: root.maybeUse();

    function maybeUse() {
        if (root.layerEffect)
            box.layer.effect = shaderEffect
    }


    Rectangle {
        color: "red"
        anchors.left: parent.left
        anchors.top: parent.top
        width: 100
        height: 100
        z: 1
    }

    Rectangle {
        color: "#00ff00"
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 100
        height: 100
        z: 3
    }

    Rectangle {
        id: box
        color: "blue"
        anchors.fill: parent
        anchors.margins: 10
        layer.enabled: true
        z: 2
    }

}
