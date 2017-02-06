import QtQuick 2.8

Item {
    width: 300
    height: 300
    Image {
        id: image
        source: "colors.png"
        visible: false
    }
    ShaderEffect {
        anchors.fill: parent
        property variant source: image
        mesh: BorderImageMesh {
            border.left: 30
            border.right: 30
            border.top: 30
            border.bottom: 30
            size: image.sourceSize
        }
    }
}
