import QtQuick 2.0

Rectangle {
    width: 200; height: 200

    Image {
        objectName: "tiling"; anchors.fill: parent
        source: "green.png"; fillMode: Image.TileVertically
    }
}

