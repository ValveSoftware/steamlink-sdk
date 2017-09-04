import QtQuick 2.0

Rectangle {
    id: root
    width: 320
    height: 480

    Text {
        id: textItem
        text: "Text Item"
        visible: false
    }

    ShaderEffect {
        width: root.width / 2
        height: root.height / 2
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        property variant source: ShaderEffectSource {
            sourceItem: ShaderEffect {
                width: root.width / 2
                height: root.height /2
                property variant source: ShaderEffectSource { sourceItem: textItem }
            }
        }
    }
}

