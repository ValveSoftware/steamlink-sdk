import QtQuick 2.0

Rectangle {
    Flipable {
        id: flipable
    }
    Rectangle {
        visible: flipable.side == Flipable.Front
    }
}
