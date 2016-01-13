import QtQuick 2.0

Rectangle {
    id: root
    width: 320
    height: 480

    property bool useSmooth: false

    property variant fillModes: [
        Image.Stretch,
        Image.PreserveAspectFit,
        Image.PreserveAspectCrop,
        Image.Tile,
        Image.TileVertically,
        Image.TileHorizontally
    ]

    Grid {
        columns: 2
        rows: 2

        Grid {
            width: 160
            height: 240
            columns: 2
            rows: 3
            Repeater {
                model: 6
                Image {
                    width: 80
                    height: 80
                    source: "../shared/tile.png"
                    fillMode: fillModes[index]
                }
            }
        }

        Grid {
            width: 160
            height: 240
            columns: 2
            rows: 3
            Repeater {
                model: 6
                Image {
                    width: 80
                    height: 80
                    source: "../shared/tile.png"
                    fillMode: fillModes[index]
                    smooth: true
                }
            }
        }

        Grid {
            width: 160
            height: 240
            columns: 2
            rows: 3
            Repeater {
                model: 6
                Item {
                    width: 80
                    height: 80
                    clip: true
                    Image {
                        width: 80
                        height: 80
                        source: "../shared/tile.png"
                        fillMode: fillModes[index]
                        scale: 1.5
                    }
                }
            }
        }

        Grid {
            width: 160
            height: 240
            columns: 2
            rows: 3
            Repeater {
                model: 6
                Item {
                    width: 80
                    height: 80
                    clip: true
                    Image {
                        width: 80
                        height: 80
                        source: "../shared/tile.png"
                        fillMode: fillModes[index]
                        smooth: true
                        scale: 1.5
                    }
                }
            }
        }
    }

}
