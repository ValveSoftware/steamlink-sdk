import QtQuick 2.0

Rectangle {
    color: "white"
    width: 800
    height: 600

    ListModel {
        id: model
        ListElement { name: "1" }
        ListElement { name: "2" }
        ListElement { name: "3" }
        ListElement { name: "4" }
        ListElement { name: "5" }
        ListElement { name: "6" }
        ListElement { name: "7" }
        ListElement { name: "8" }
        ListElement { name: "9" }
    }

    Component {
        id: verticalDelegate
        FocusScope {
            id: root
            width: 50; height: 50;
            Keys.onDigit9Pressed: console.log("Error - " + name)
            Rectangle {
                focus: true
                Keys.onDigit9Pressed: console.log(name)
                width: 50; height: 50;
                color: root.ListView.isCurrentItem?"red":"green"
                Text { text: name; anchors.centerIn: parent }
            }
        }
    }

    ListView {
        width: 800; height: 50; orientation: "Horizontal"
        focus: true
        model: model
        delegate: verticalDelegate
        preferredHighlightBegin: 100
        preferredHighlightEnd: 100
        highlightRangeMode: "StrictlyEnforceRange"
    }


    Text {
        y: 100; x: 50
        text: "Currently selected element should be red\nPressing \"9\" should print the number of the currently selected item\nBe sure to scroll all the way to the right, pause, and then all the way to the left."
    }
}
