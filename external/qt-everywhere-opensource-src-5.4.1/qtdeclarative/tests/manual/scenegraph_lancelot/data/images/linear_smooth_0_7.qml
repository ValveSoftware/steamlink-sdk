import QtQuick 2.0

Rectangle {
    width: 320
    height: 480
    smooth: true
    Text{
        text: "scale 0.7"
        z: 1
    }
    Image{
        width: 320
        height: 480
        id: bwLinear
        source: "../shared/bw_1535x2244.jpg"
        anchors.fill: parent
        sourceSize.height: 2244
        sourceSize.width: 1535
        scale: 0.7
    }
}
