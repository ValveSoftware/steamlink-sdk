import QtQuick 2.0

Rectangle {
    width: 240
    height: 320

    GridView {
        id: grid

        property real itemZ: 1
        property real headerZ: 1
        property real footerZ: 1
        property real highlightZ: 0

        anchors.fill: parent
        objectName: "grid"
        model: ListModel { ListElement { text: "text" } }
        currentIndex: 0

        delegate: Text {
            objectName: "wrapper"
            font.pointSize: 20
            text: index
        }

        header: Rectangle {
            width: 240
            height: 30
        }

        footer: Rectangle {
            width: 240
            height: 30
        }
    }
}

