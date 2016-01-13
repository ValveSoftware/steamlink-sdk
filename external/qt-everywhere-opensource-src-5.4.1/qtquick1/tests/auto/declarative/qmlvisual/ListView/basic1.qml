import QtQuick 1.0

Rectangle {
    color: "blue"
    width: 200
    height: 300
    id: page
    ListView {
        anchors.fill: parent
        delegate: Rectangle {
            color: "red"
            width: 100
            height: 100
            Rectangle {
                anchors.centerIn: parent
                width: 60
                height: 60
                color: name
            }
        }
        model: ListModel {
            ListElement {
                name: "palegoldenrod"
            }
            ListElement {
                name: "lightsteelblue"
            }
        }
    }
}
