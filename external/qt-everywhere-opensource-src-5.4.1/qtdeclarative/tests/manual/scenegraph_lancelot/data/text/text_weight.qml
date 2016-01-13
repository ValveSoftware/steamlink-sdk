import QtQuick 2.0

//vary font.weight

Item {
    width: 320
    height: 480
    Text {
        id: text_0000
        anchors.top: parent.top
        anchors.left: parent.left
        width: 150
        text: "The quick brown fox jumps over the lazy dog. 0123456789"
        font.weight: Font.Light
        color: "black"
        font.family: "Meera"
        font.pointSize: 10
        wrapMode: Text.Wrap
    }
    Text {
        id: text_0001
        anchors.top: text_0000.bottom
        anchors.left: parent.left
        width: 150
        text: "The quick brown fox jumps over the lazy dog. 0123456789"
        font.weight: Font.Normal
        color: "black"
        font.family: "Mallige"
        font.pointSize: 10
        wrapMode: Text.Wrap
    }
    Text {
        id: text_0002
        width: 150
        anchors.top: text_0001.bottom
        anchors.left: text_0001.left
        text: "The quick brown fox jumps over the lazy dog. 0123456789"
        font.weight: Font.DemiBold
        color: "black"
        font.family: "Norasi"
        font.pointSize: 10
        wrapMode: Text.Wrap
    }
    Text {
        id: text_0003
        width: 150
        anchors.top: text_0002.bottom
        anchors.left: text_0002.left
        text: "The quick brown fox jumps over the lazy dog. 0123456789"
        font.weight: Font.Bold
        color: "black"
        font.family: "Purisa"
        font.pointSize: 10
        wrapMode: Text.Wrap
    }
    Text {
        id: text_0004
        width: 150
        anchors.top: text_0003.bottom
        anchors.left: text_0003.left
        text: "The quick brown fox jumps over the lazy dog. 0123456789"
        font.weight: Font.Black
        color: "black"
        font.family: "Saab"
        font.pointSize: 10
        wrapMode: Text.Wrap
    }
    Text {
        id: text_0005
        width: 150
        anchors.top: text_0004.bottom
        anchors.left: text_0004.left
        text: "The quick brown fox jumps over the lazy dog. 0123456789"
        font.weight: Font.Normal
        color: "black"
        font.family: "Sawasdee"
        font.pointSize: 10
        wrapMode: Text.Wrap
    }
    Text {
        id: text_0006
        width: 150
        anchors.top: text_0005.bottom
        anchors.left: text_0005.left
        text: "The quick brown fox jumps over the lazy dog. 0123456789"
        font.weight: Font.Normal
        color: "black"
        font.family: "Symbol"
        font.pointSize: 10
        wrapMode: Text.Wrap
    }
    Text {
        id: text_0007
        width: 150
        anchors.top: text_0006.bottom
        anchors.left: text_0006.left
        text: "The quick brown fox jumps over the lazy dog. 0123456789"
        font.weight: Font.Normal
        color: "black"
        font.family: "UnBatang"
        font.pointSize: 10
        wrapMode: Text.Wrap
    }
}
