import QtQuick 2.2

Rectangle {
    width: 320
    height: 480

    color: "red"

    Item {
        x: 80
        y: 80

        BorderImage {
            source: "../shared/world.png"

            width: 10
            height: 10

            antialiasing: true

            horizontalTileMode: BorderImage.Repeat
            verticalTileMode: BorderImage.Repeat

            smooth: false

            border.bottom: 5
            border.left: 0
            border.right: 0
            border.top: 5
        }
    }
}
