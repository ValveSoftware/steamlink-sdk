import QtQuick 2.0

Item {
    width: 200; height: 200

    Rectangle {
        x: 50; y: 50; width: 50; height: 50; color: "red"

        SequentialAnimation on rotation {
            NumberAnimation {
                from: 0; to: 90; duration: 100
                loops: 3
            }
        }
    }
}
