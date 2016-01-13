import QtQuick 2.0

Item {
    ListModel {
        id: fruitModel
        ListElement {
            name: "Apple"
            cost: 2.45
        }
        ListElement {
            name: "Orange"
            cost: 3.25
        }
        ListElement {
            name: "Banana"
            cost: 1.95
        }
    }

    Component {
        id: fruitDelegate
        Item {
            width: 200; height: 50
            Text { text: name }
            Text { text: '$'+cost; anchors.right: parent.right }
            MouseArea {
                anchors.fill: parent
                onClicked: fruitModel.remove(index)
            }
        }
    }

    ListView {
        model: fruitModel
        delegate: fruitDelegate
        anchors.fill: parent
    }
}
