import QtQuick 2.0

Rectangle {
    property variant myInput: input

    width: 800; height: 600; color: "blue"

    Item {
        id: firstItem;
        KeyNavigation.right: input
    }

    TextEdit { id: input; focus: true
        KeyNavigation.left: firstItem
        KeyNavigation.right: lastItem
        KeyNavigation.up: firstItem
        KeyNavigation.down: lastItem
        text: "a"
    }
    Item {
        id: lastItem
        KeyNavigation.left: input
    }
}
