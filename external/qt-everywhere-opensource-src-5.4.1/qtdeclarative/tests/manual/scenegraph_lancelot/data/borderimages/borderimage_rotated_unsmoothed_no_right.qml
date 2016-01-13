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

            width: 8
            height: 8

            antialiasing: true

            horizontalTileMode: BorderImage.Repeat
            verticalTileMode: BorderImage.Repeat

            smooth: false
            rotation: 10
            scale: 10

            border.bottom: 1
            border.left: 1
            border.right: 0
            border.top: 1
        }
    }
}
