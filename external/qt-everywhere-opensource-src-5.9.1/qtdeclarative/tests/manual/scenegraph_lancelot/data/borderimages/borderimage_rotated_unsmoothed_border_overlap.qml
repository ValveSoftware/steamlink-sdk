import QtQuick 2.2

Rectangle {
    width: 320
    height: 480

    color: "red"

    Item {
        x: 80
        y: 80

        BorderImage {
            source: "../shared/uniquepixels.png"

            antialiasing: true

            horizontalTileMode: BorderImage.Repeat
            verticalTileMode: BorderImage.Repeat

            smooth: false
            rotation: 10
            scale: 10

            border.bottom: 0
            border.left: 3
            border.right: 3
            border.top: 0
        }
    }
}
