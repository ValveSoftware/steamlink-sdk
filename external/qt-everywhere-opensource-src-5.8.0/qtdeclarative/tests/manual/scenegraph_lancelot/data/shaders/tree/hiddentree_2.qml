import QtQuick 2.0

Item {
    width: 320
    height: 480

    Rectangle {
        visible: false
        Item {
            Text {
                id: text
                font.pixelSize:  80
                text: "Shaderz!"
                color: "black"
                Rectangle {
                    width: 50
                    height: 50
                    color: "red"
                    anchors.centerIn: parent
                    transform: Rotation { angle: 90 }
                }
            }
        }
    }

    ShaderEffectSource {
        width: text.width
        height: text.height
        anchors.centerIn: parent
        sourceItem: text
    }
}
