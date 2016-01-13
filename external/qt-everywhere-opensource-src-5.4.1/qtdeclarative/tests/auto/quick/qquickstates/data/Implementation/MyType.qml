import QtQuick 2.0

Item {
    Column {
        anchors.centerIn: parent
        Image { id: image1; objectName: "image1" }
        Image { id: image2; objectName: "image2" }
        Image { id: image3; objectName: "image3" }
    }

    states: State {
        name: "SetImageState"
        PropertyChanges {
            target: image1
            source: "images/qt-logo.png"
        }
        PropertyChanges {
            target: image2
            source: "images/" + "qt-logo.png"
        }
        PropertyChanges {
            target: image3
            source: "images/" + (true ? "qt-logo.png" : "")
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: parent.state = "SetImageState"
    }

}
