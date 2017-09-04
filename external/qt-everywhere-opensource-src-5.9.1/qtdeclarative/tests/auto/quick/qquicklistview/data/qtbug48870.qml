import QtQuick 2.6

Rectangle {
    width: 500
    height: 500
    color: "blue"

    ListView {
        objectName: "list"
        anchors.fill: parent
        model: testModel

        delegate: Rectangle {
            height: 50
            width: ListView.view ? ListView.view.width : height
            color: "green"

            Text {
                anchors.centerIn: parent
                text: "Item " + index
            }
        }
    }
}
