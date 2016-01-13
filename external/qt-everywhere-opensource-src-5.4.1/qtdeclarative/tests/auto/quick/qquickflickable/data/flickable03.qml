import QtQuick 2.0

Flickable {
    property bool movingInContentX: true
    property bool movingInContentY: true
    property bool draggingInContentX: true
    property bool draggingInContentY: true

    width: 100; height: 400
    contentWidth: column.width; contentHeight: column.height

    onContentXChanged: {
        movingInContentX = movingInContentX && movingHorizontally
        draggingInContentX = draggingInContentX && draggingHorizontally
    }
    onContentYChanged: {
        movingInContentY = movingInContentY && movingVertically
        draggingInContentY = draggingInContentY && draggingVertically
    }

    Column {
        id: column
        Repeater {
            model: 20
            Rectangle { width: 200; height: 300; border.width: 1; color: "yellow" }
        }
    }
}
