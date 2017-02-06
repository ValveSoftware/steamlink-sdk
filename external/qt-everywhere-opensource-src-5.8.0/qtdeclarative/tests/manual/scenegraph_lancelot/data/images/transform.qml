import QtQuick 2.0

Rectangle {
    width: 320
    height: 480
    color: "white"
    Image {
        id: wooohooo
        x: 50
        y: 120
        width: 256
        height: 240
        source: "../shared/winter.png"
        rotation: 30
        scale: 1.25
        transform: [
            Rotation { origin { x: 128; y: 120 } axis { x: 0; y: 1; z: 0 } angle: 60 }
            , Translate { x: 10; y: 0 }
        ]
        Rectangle {
            width: 50
            height: 50
            color: "steelBlue"
            MouseArea {
                anchors.fill: parent
                drag.target: parent
                drag.axis: Drag.XAndYAxis
                drag.minimumX: 0
                drag.maximumX: wooohooo.width - parent.width
                drag.minimumY: 0
                drag.maximumY: wooohooo.height - parent.height
            }
        }
    }
}

