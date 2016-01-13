import QtQuick 2.0

ListView {
    width: 240
    height: 320

    function switchDelegates() {
        section.delegate = section.delegate === delegate1
                ? delegate2
                : delegate1
    }

    Component {
        id: delegate1

        Rectangle {
            objectName: "section1"
            color: "lightsteelblue"
            border.width: 1;
            width: 240
            height: 25

            Text {
                anchors.centerIn: parent
                text: section
            }
        }
    }
    Component {
        id: delegate2

        Rectangle {
            objectName: "section2"
            color: "yellow"
            border.width: 1;
            width: 240
            height: 50

            Text {
                anchors.centerIn: parent
                text: section
            }
        }
    }

    section.property: "modelData"
    section.delegate: delegate1

    model: 20
    delegate: Rectangle {
        objectName: "item"
        border.width: 1
        width: 240
        height: 25

        Text {
            anchors.centerIn: parent
            text: modelData
        }
    }
}
