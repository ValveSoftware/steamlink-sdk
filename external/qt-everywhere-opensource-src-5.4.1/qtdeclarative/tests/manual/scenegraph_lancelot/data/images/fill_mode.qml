import QtQuick 2.0

Rectangle {
    width: 320
    height: 480
    smooth: true
    Rectangle{
        id: rect0_0
        width: 160
        height: 160
        x: 0
        y: 0
        Text{
            text: "Stretch"
            x: 20
            y: 20
            color: "red"
            font.family: "Helvetica"
            font.pointSize: 24
            z: 1
        }
        Image{
            id: bwLinear1
            width: 160
            height: 160
            source: "../shared/bw_1535x2244.jpg"
            sourceSize.width: 1535
            sourceSize.height: 2244
        }
    }
    Rectangle{
        id: rect0_1
        width: 160
        height: 160
        x: 180
        y: 0
        Text{
            text: "Preserve\naspect\nfit"
            x: 20
            y: 20
            color: "red"
            font.family: "Helvetica"
            font.pointSize: 24
            z: 1
        }
        Image{
            id: bwLinear2
            width: 160
            height: 160
            source: "../shared/bw_1535x2244.jpg"
            fillMode: Image.PreserveAspectFit
            sourceSize.width: 1535
            sourceSize.height: 2244
        }
    }
    Rectangle{
        id: rect1_0
        width: 160
        height: 160
        x: 0
        y: 160
        Text{
            text: "Preserve\naspect\ncrop"
            x: 20
            y: 20
            color: "red"
            font.family: "Helvetica"
            font.pointSize: 24
            z: 1
        }
        Image{
            id: bwLinear3
            width: 160
            height: 160
            source: "../shared/bw_1535x2244.jpg"
            fillMode: Image.PreserveAspectCrop
            sourceSize.width: 1535
            sourceSize.height: 2244
        }

    }
    Rectangle{
        id: rect1_1
        width: 160
        height: 160
        x: 180
        y: 160
        Text{
            text: "Tile"
            x: 20
            y: 20
            color: "red"
            font.family: "Helvetica"
            font.pointSize: 24
            z: 1
        }
        Image{
            id: bwLinear4
            width: 160
            height: 160
            source: "../shared/bw_1535x2244.jpg"
            fillMode: Image.Tile
            sourceSize.width: 1535
            sourceSize.height: 2244
        }
    }

    Rectangle{
        id: rect2_0
        width: 160
        height: 160
        x: 0
        y: 320
        Text{
            text: "Tile\nvertically"
            x: 20
            y: 20
            color: "red"
            font.family: "Helvetica"
            font.pointSize: 24
            z: 1
        }
        Image{
            id: bwLinear5
            width: 160
            height: 160
            source: "../shared/bw_1535x2244.jpg"
            fillMode: Image.TileVertically
            sourceSize.width: 1535
            sourceSize.height: 2244
        }
    }
    Rectangle{
        id: rect2_1
        width: 160
        height: 160
          x: 180
          y: 320
          Text{
              text: "Tile horizontally"
              x: 20
              y: 20
              color: "red"
              font.family: "Helvetica"
              font.pointSize: 24
              z: 1
          }
          Image{
              id: bwLinear6
              width: 160
              height: 160
              source: "../shared/bw_1535x2244.jpg"
              fillMode: Image.TileHorizontally
              sourceSize.width: 1535
              sourceSize.height: 2244
          }
    }






}

