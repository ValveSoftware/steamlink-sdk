import QtQuick 1.0

/*
    Currently selected element should be red
    Pressing "9" should print the number of the currently selected item
    Be sure to scroll all the way to the right, pause, and then all the way to the left
*/
Rectangle {
    color: "white"
    width: 400
    height: 100

    ListModel {
        id: model
        ListElement { name: "red" }
        ListElement { name: "orange" }
        ListElement { name: "yellow" }
        ListElement { name: "green" }
        ListElement { name: "cyan" }
        ListElement { name: "blue" }
        ListElement { name: "indigo" }
        ListElement { name: "violet" }
        ListElement { name: "pink" }
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
                color: root.ListView.isCurrentItem?"black":name
            }
        }
    }

    ListView {
        width: 800; height: 50; orientation: "Horizontal"
        focus: true
        model: model
        delegate: verticalDelegate
        preferredHighlightBegin: 100
        preferredHighlightEnd: 101
        highlightRangeMode: ListView.StrictlyEnforceRange
    }


}
