import QtQuick 2.0

Rectangle {
    width: 240
    height: 320

    GridView {
        id: grid

        property real itemZ: 342
        property real headerZ: 341
        property real footerZ: 340
        property real highlightZ: 339

        anchors.fill: parent
        objectName: "grid"
        model: ListModel { ListElement { text: "text" } }
        currentIndex: 0

        delegate: Text {
            objectName: "wrapper"
            font.pointSize: 20
            text: index
            z: grid.itemZ
        }

        header: Rectangle {
            width: 240
            height: 30
            z: grid.headerZ
        }

        footer: Rectangle {
            width: 240
            height: 30
            z: grid.footerZ
        }

        highlight: Rectangle {
            width: 240
            height: 30
            z: grid.highlightZ
        }
    }
}

