import QtQuick 1.0

Item {
    property alias horizontalMode: image.horizontalTileMode
    property alias verticalMode: image.verticalTileMode
    property alias source: image.source
    property alias antialiased: image.smooth

    property int minWidth
    property int minHeight
    property int maxWidth
    property int maxHeight
    property int margin

    id: container
    width: maxWidth; height: maxHeight

    BorderImage {
        id: image; x: container.width / 2 - width / 2; y: container.height / 2 - height / 2

        SequentialAnimation on width {
            loops: Animation.Infinite
            NumberAnimation { from: container.minWidth; to: container.maxWidth; duration: 600; easing.type: "InOutQuad"}
            NumberAnimation { from: container.maxWidth; to: container.minWidth; duration: 600; easing.type: "InOutQuad" }
        }

        SequentialAnimation on height {
            loops: Animation.Infinite
            NumberAnimation { from: container.minHeight; to: container.maxHeight; duration: 600; easing.type: "InOutQuad"}
            NumberAnimation { from: container.maxHeight; to: container.minHeight; duration: 600; easing.type: "InOutQuad" }
        }

        border.top: container.margin
        border.left: container.margin
        border.bottom: container.margin
        border.right: container.margin
    }
}
