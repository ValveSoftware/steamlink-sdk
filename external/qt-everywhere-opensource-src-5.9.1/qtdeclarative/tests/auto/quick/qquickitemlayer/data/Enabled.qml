import QtQuick 2.0

Item
{
    width: 200
    height: 200

    Item {
        width: 20
        height: 20
        scale: 10

        layer.enabled: true
        anchors.centerIn: parent

        Rectangle {
            width: 20
            height: 20
            gradient: Gradient {
                GradientStop { position: 0; color: "white" }
                GradientStop { position: 1; color: "black" }
            }
        }
    }
}
