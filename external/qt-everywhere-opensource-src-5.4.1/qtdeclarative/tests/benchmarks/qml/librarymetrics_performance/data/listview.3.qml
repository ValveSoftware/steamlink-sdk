import QtQuick 2.0

Item {
    width: 320
    height: 480

    ListView {
        anchors.fill: parent
        model: simpleModel
        delegate: Text {
            text: name
        }
    }

    ListModel {
        id: simpleModel
        ListElement {
            name: "first"
        }
        ListElement {
            name: "second"
        }
        ListElement {
            name: "third"
        }
    }
}
