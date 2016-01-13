import QtQuick 1.0

Rectangle {
    id: rect
    width: 120; height: 200; color: "white"
    Text { id: text1; anchors.right: text2.right; text: "Hello" }
    Text { id: text2; anchors.right: text1.right; anchors.rightMargin: 10; text: "World" }
}
