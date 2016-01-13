import QtQuick 2.0

Rectangle {
    width: 250
    height: 250

    ListView {
        anchors.fill: parent
        model: ListModel {
            ListElement { text: "A" }
            ListElement { text: "B" }
        }

        populate: Transition {
            SpringAnimation { properties: "x"; from: 0; to: 100; spring: 4; damping: 0.3 }
        }

        delegate: Text {
            text: "Test"
        }
    }
}
