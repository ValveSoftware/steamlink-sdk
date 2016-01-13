import QtQuick 1.0

Rectangle {
    width: 800; height: 600

    Image {
        objectName: "vTiling"; height: 550; width: 200
        source: "green.png"; fillMode: Image.TileVertically
    }

    Image {
        objectName: "hTiling"; x: 225; height: 250; width: 550
        source: "green.png"; fillMode: Image.TileHorizontally
    }
}

