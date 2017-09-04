import QtQuick 2.0

Item {
    width: 640
    height: 480

    function setModel() {
        listView.model = listModel1
    }

    ListModel {
        id: listModel1
        ListElement { text: "Apple" }
        ListElement { text: "Banana" }
        ListElement { text: "Orange" }
        ListElement { text: "Coconut" }
    }

    Rectangle {
        width: 200
        height: listView.contentHeight
        color: "yellow"
        anchors.centerIn: parent

        ListView {
            id: listView
            objectName: "listview"
            anchors.fill: parent

            delegate: Item {
                width: 200
                height: 20
                Text { text: model.text; anchors.centerIn: parent }
            }
        }
    }
}
