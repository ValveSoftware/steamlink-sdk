import QtQuick 2.0

Rectangle {
    width: 240
    height: 320

    GridView {
        id: grid

        property real itemZ: 241
        property real headerZ: 242
        property real footerZ: 243
        property real highlightZ: 244

        anchors.fill: parent
        objectName: "grid"
        model: ListModel { ListElement { text: "text" } }
        currentIndex: 0

        delegate: Text {
            objectName: "wrapper"
            font.pointSize: 20
            text: index
            z: 241
        }

        header: Rectangle {
            width: 240
            height: 30
            z: 242
        }

        footer: Rectangle {
            width: 240
            height: 30
            z: 243
        }

        highlight: Rectangle {
            width: 240
            height: 30
            z: 244
        }
    }
}

