import QtQuick 2.0

Item {
    width: 320
    height: 480

    Rectangle {
        id: foo
        width: 200
        height: 200
        radius: 100
        anchors.centerIn: parent
        gradient: Gradient {
            GradientStop { position: 0; color: "red" }
            GradientStop { position: 0.5; color: "white" }
            GradientStop { position: 1; color: "blue" }
        }
        ShaderEffectSource {
            id: buffer
            anchors.fill: parent
            sourceItem: foo
            live: false
            smooth: true
            rotation: 45
            recursive: true
        }
    }
}
