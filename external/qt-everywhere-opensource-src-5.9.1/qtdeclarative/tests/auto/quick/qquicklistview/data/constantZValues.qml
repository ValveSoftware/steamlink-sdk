import QtQuick 2.0

Rectangle {
    width: 240
    height: 320

    ListView {
        id: list

        property real itemZ: 241
        property real headerZ: 242
        property real footerZ: 243
        property real highlightZ: 244
        property real sectionZ: 245

        anchors.fill: parent
        objectName: "list"
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

        section.property: "text"
        section.delegate: Text {
            objectName: "section"
            font.pointSize: 20
            text: section
            z: 245
        }
    }
}

