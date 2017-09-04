import QtQuick 2.0

Rectangle {
    width: 240
    height: 320

    ListView {
        id: list

        property real itemZ: 1
        property real headerZ: 1
        property real footerZ: 1
        property real highlightZ: 0
        property real sectionZ: 2

        anchors.fill: parent
        objectName: "list"
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

        highlight: Rectangle {
            width: 240
            height: 30
        }

        section.property: "text"
        section.delegate: Text {
            objectName: "section"
            font.pointSize: 20
            text: section
        }
    }
}
