import QtQuick 2.0

//vary font scale without smoothing

Item {
    width: 320
    height: 480
    property bool smoothing: false
    Text {
        id: text_0000
        x: 0
        y: 0
        width: 150
        text: "The quick brown fox jumps over the lazy dog. 0123456789. style: Normal"
        style: Text.Normal
        color: "black"
        font.family: "Arial"
        font.pointSize: 12
        wrapMode: Text.Wrap
        transform: Scale{origin.x: 0 ; origin.y: 0; xScale: 0.3}
        smooth: smoothing
    }
    Text {
        id: text_0001
        anchors.top: text_0000.bottom
        anchors.left: text_0000.left
        width: 150
        text: "The quick brown fox jumps over the lazy dog. 0123456789. style: Normal"
        style: Text.Normal
        color: "black"
        font.family: "Arial"
        font.pointSize: 12
        wrapMode: Text.Wrap
        transform: Scale{origin.x: 0 ; origin.y: 0; xScale: 0.4}
        smooth: smoothing
    }
    Text {
        id: text_0002
        anchors.top: text_0001.bottom
        anchors.left: text_0001.left
        width: 150
        text: "The quick brown fox jumps over the lazy dog. 0123456789. style: Normal"
        style: Text.Normal
        color: "black"
        font.family: "Arial"
        font.pointSize: 12
        wrapMode: Text.Wrap
        transform: Scale{origin.x: 0 ; origin.y: 0; xScale: 0.5}
        smooth: smoothing
    }
    Text {
        id: text_0003
        anchors.top: text_0002.bottom
        anchors.left: text_0002.left
        width: 150
        text: "The quick brown fox jumps over the lazy dog. 0123456789. style: Normal"
        style: Text.Normal
        color: "black"
        font.family: "Arial"
        font.pointSize: 12
        wrapMode: Text.Wrap
        transform: Scale{origin.x: 0 ; origin.y: 0; xScale: 0.6}
        smooth: smoothing
    }
    Text {
        id: text_0004
        anchors.top: text_0003.bottom
        anchors.left: text_0003.left
        width: 150
        text: "The quick brown fox jumps over the lazy dog. 0123456789. style: Normal"
        style: Text.Normal
        color: "black"
        font.family: "Arial"
        font.pointSize: 12
        wrapMode: Text.Wrap
        transform: Scale{origin.x: 0 ; origin.y: 0; xScale: 0.8; }
        smooth: smoothing
    }
}
