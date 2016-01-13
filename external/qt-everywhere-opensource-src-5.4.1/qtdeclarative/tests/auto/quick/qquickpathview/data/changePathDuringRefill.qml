import QtQuick 2.3

PathView {
    id: view
    objectName: "pathView"
    width: 100
    height: delegateHeight * pathItemCount
    model: ["A", "B", "C"]
    pathItemCount: 3
    anchors.centerIn: parent

    property int delegateHeight: 0

    activeFocusOnTab: true
    Keys.onDownPressed: view.incrementCurrentIndex()
    Keys.onUpPressed: view.decrementCurrentIndex()
    preferredHighlightBegin: 0.5
    preferredHighlightEnd: 0.5

    delegate: Rectangle {
        objectName: "delegate" + modelData
        width: view.width
        height: textItem.height
        border.color: "red"

        onHeightChanged: {
            if (index == 0)
                view.delegateHeight = textItem.height
        }

        Text {
            id: textItem
            text: modelData
        }
    }

    path: Path {
        startX: view.width / 2
        startY: 0
        PathLine {
            x: view.width / 2
            y: view.pathItemCount * view.delegateHeight
        }
    }
}
