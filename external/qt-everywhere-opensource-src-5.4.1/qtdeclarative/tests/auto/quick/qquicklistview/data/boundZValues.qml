import QtQuick 2.0

Rectangle {
    width: 240
    height: 320

    ListView {
        id: list

        property real itemZ: 342
        property real headerZ: 341
        property real footerZ: 340
        property real highlightZ: 339
        property real sectionZ: 338

        anchors.fill: parent
        objectName: "list"
        model: ListModel { ListElement { text: "text" } }
        currentIndex: 0

        delegate: Text {
            objectName: "wrapper"
            font.pointSize: 20
            text: index
            z: list.itemZ
        }

        header: Rectangle {
            width: 240
            height: 30
            z: list.headerZ
        }

        footer: Rectangle {
            width: 240
            height: 30
            z: list.footerZ
        }

        highlight: Rectangle {
            width: 240
            height: 30
            z: list.highlightZ
        }

        section.property: "text"
        section.delegate: Text {
            objectName: "section"
            font.pointSize: 20
            text: section
            z: list.sectionZ
        }
    }
}

