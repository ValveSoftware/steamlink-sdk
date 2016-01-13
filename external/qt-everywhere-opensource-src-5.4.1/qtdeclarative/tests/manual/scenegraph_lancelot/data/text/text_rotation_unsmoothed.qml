import QtQuick 2.0

//vary font rotation without smoothing

Item {
    width: 320
    height: 480
    property bool smoothing: false
    Text {
        id: text_0000
        x: 0
        y: 0
        width: 150
        height: 50
        text: "10 degrees. The quick brown fox jumps over the lazy dog. 0123456789."
        style: Text.Normal
        color: "black"
        font.family: "Arial"
        font.pointSize: 12
        wrapMode: Text.NoWrap
        transform: Rotation{origin.x: 25 ; origin.y: 25; angle: 10}
        smooth: smoothing
    }
    Text {
        id: text_0001
        anchors.top: text_0000.bottom
        anchors.left: text_0000.left
        width: 150
        height: 50
        text: "20 degrees. The quick brown fox jumps over the lazy dog. 0123456789. style: Normal"
        style: Text.Normal
        color: "black"
        font.family: "Arial"
        font.pointSize: 12
        wrapMode: Text.NoWrap
        transform: Rotation{origin.x: 30 ; origin.y: 25; angle: 20}
        smooth: smoothing
    }
    Text {
        id: text_0002
        anchors.top: text_0001.bottom
        anchors.left: text_0001.left
        width: 150
        height: 50
        text: "30 degrees. The quick brown fox jumps over the lazy dog. 0123456789. style: Normal"
        style: Text.Normal
        color: "black"
        font.family: "Arial"
        font.pointSize: 12
        wrapMode: Text.NoWrap
         transform: Rotation{origin.x: 35 ; origin.y: 25; angle: 30}
        smooth: smoothing
    }
    Text {
        id: text_0003
        anchors.top: text_0002.bottom
        anchors.left: text_0002.left
        width: 150
        height: 50
        text: "40 degrees. The quick brown fox jumps over the lazy dog. 0123456789. style: Normal"
        style: Text.Normal
        color: "black"
        font.family: "Arial"
        font.pointSize: 12
        wrapMode: Text.NoWrap
        transform: Rotation{origin.x: 40 ; origin.y: 25; angle: 40}
        smooth: smoothing
    }
    Text {
        id: text_0004
        anchors.top: text_0003.bottom
        anchors.left: text_0003.left
        width: 150
        height: 50
        text: "50 degrees. The quick brown fox jumps over the lazy dog. 0123456789. style: Normal"
        style: Text.Normal
        color: "black"
        font.family: "Arial"
        font.pointSize: 12
        wrapMode: Text.NoWrap
        transform: Rotation{origin.x: 45 ; origin.y: 25; angle: 50}
        smooth: smoothing
    }
    Text {
        id: text_0005
        anchors.top: text_0004.bottom
        anchors.left: text_0004.left
        width: 150
        height: 50
        text: "60 degrees. The quick brown fox jumps over the lazy dog. 0123456789. style: Normal"
        style: Text.Normal
        color: "black"
        font.family: "Arial"
        font.pointSize: 12
        wrapMode: Text.NoWrap
        transform: Rotation{origin.x: 50 ; origin.y: 25; angle: 60}
        smooth: smoothing
    }
    Text {
        id: text_0006
        anchors.top: text_0005.bottom
        anchors.left: text_0005.left
        width: 150
        height: 50
        text: "70 degrees. The quick brown fox jumps over the lazy dog. 0123456789. style: Normal"
        style: Text.Normal
        color: "black"
        font.family: "Arial"
        font.pointSize: 12
        wrapMode: Text.NoWrap
        transform: Rotation{origin.x: 30 ; origin.y: 25; angle: 70}
        smooth: smoothing
    }
    Text {
        id: text_0007
        anchors.top: text_0006.bottom
        anchors.left: text_0006.left
        width: 150
        height: 50
        text: "80 degrees. The quick brown fox jumps over the lazy dog. 0123456789. style: Normal"
        style: Text.Normal
        color: "black"
        font.family: "Arial"
        font.pointSize: 12
        wrapMode: Text.NoWrap
        transform: Rotation{origin.x: 20 ; origin.y: 25; angle: 80}
        smooth: smoothing
    }
    Text {
        id: text_0008
        anchors.top: text_0007.bottom
        anchors.left: text_0007.left
        width: 150
        height: 50
        text: "90 degrees. The quick brown fox jumps over the lazy dog. 0123456789. style: Normal"
        style: Text.Normal
        color: "black"
        font.family: "Arial"
        font.pointSize: 12
        wrapMode: Text.NoWrap
        transform: Rotation{origin.x: 10 ; origin.y: 25; angle: 90}
        smooth: smoothing
    }
}
