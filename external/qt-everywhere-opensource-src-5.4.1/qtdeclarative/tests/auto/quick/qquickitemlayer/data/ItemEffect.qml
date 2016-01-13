import QtQuick 2.0

Item {
    width: 200
    height: 200
    Rectangle {
        anchors.fill: parent
        anchors.margins: 99
        gradient: Gradient {
            GradientStop { position: 0.3; color: "red" }
            GradientStop { position: 0.7; color: "blue" }
        }
        layer.enabled: true
        layer.effect: Item {
            property variant source
            ShaderEffect {
                anchors.fill: parent
                anchors.margins: -99
                property variant source: parent.source
            }
        }
    }
}
