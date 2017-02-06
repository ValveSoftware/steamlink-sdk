import QtQuick 2.0

Rectangle {
    color: "#ffffff"
    width: 320; height: 240
    Rectangle {
        id: brother
        objectName: "brother"
        color: "lightgreen"
        x: 200; y: 100
        width: 120; height: 120
    }
    MouseArea {
        id: mouseRegion
        objectName: "mouseregion"

        property int x1
        property int y1
        property int x2
        property int y2
        property bool emitPositionChanged: false
        property bool mouseMatchesPos: true

        anchors.fill: brother
        onPressed: {
            if (mouse.x != mouseX || mouse.y != mouseY)
                mouseMatchesPos = false
            x1 = mouseX; y1 = mouseY
            anchors.fill = parent
        }
        onPositionChanged: { emitPositionChanged = true }
        onMouseXChanged: {
            if (mouse.x != mouseX || mouse.y != mouseY)
                mouseMatchesPos = false
            x2 = mouseX; y2 = mouseY
        }
        onMouseYChanged: {
            if (mouse.x != mouseX || mouse.y != mouseY)
                mouseMatchesPos = false
            x2 = mouseX; y2 = mouseY
        }
    }
}
