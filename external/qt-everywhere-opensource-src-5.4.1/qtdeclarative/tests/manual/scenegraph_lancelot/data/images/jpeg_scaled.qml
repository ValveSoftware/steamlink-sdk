import QtQuick 2.0

Rectangle {
    width: 320
    height: 480
    Image{
        x: 0
        y: 0
        width: 160
        height: 240
        source: "../shared/col320x480.jpg"
    }

    Image{
        x: 180
        y: 20
        width: 113
        height: 199
        source: "../shared/col320x480.jpg"
    }

    Image{
        x: 160
        y: 240
        sourceSize.width: 160
        sourceSize.height: 240
        source: "../shared/col320x480.jpg"
    }

    Image{
        x: 20
        y: 260
        sourceSize.width: 113
        sourceSize.height: 199
        source: "../shared/col320x480.jpg"
    }

}
