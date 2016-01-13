import QtQuick 1.0
import "../../shared" 1.0

TestText {
    property color bcolor: "blue"
    font.pixelSize: 10
    text: "The quick brown fox\njumps over\nthe lazy dog."
    Rectangle { id: border; color: "transparent"; border.color: bcolor; anchors.fill: parent; opacity: 0.2 }
}
