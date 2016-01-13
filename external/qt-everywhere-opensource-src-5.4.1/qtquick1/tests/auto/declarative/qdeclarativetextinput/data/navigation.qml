import QtQuick 1.0

Rectangle {
    property variant myInput: input

    width: 800; height: 600; color: "blue"

    Item {
        id: firstItem;
        KeyNavigation.right: input
    }

    TextInput { id: input; focus: true
        text: "Needs some text"
        KeyNavigation.left: firstItem
        KeyNavigation.right: lastItem
        KeyNavigation.up: firstItem
        KeyNavigation.down: lastItem
    }
    Item {
        id: lastItem
        KeyNavigation.left: input
    }
}
