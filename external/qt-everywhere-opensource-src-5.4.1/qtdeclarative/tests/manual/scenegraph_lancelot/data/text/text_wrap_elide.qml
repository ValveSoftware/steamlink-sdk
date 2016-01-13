import QtQuick 2.0

//test single font with different elide and wrap modes

Item {
    width: 320
    height: 480
    Text {
        id: text_0000
        x: 0
        y: 0
        width: 150
        text: "The quick brown fox jumps over the lazy dog. Elide left"
        font.bold: false
        elide: Text.ElideLeft
        color: "red"
        font.family: "Arial"
        font.pointSize: 24
    }
    Text {
        id: text_0001
        anchors.top: text_0000.bottom
        anchors.left: text_0000.left
        width: 150
        text: "The quick brown fox jumps over the lazy dog. Elide middle"
        font.bold: false
        elide: Text.ElideMiddle
        color: "blue"
        font.family: "Arial"
        font.pointSize: 24
    }
    Text {
        id: text_0002
        width: 150
        anchors.top: text_0001.bottom
        anchors.left: text_0001.left
        text: "The quick brown fox jumps over the lazy dog. Elide right"
        font.bold: false
        elide: Text.ElideRight
        color: "yellow"
        font.family: "Arial"
        font.pointSize: 24
    }
    Text {
        id: text_0003
        width: 150
        anchors.top: text_0002.bottom
        anchors.left: text_0002.left
        text: "The quick brown fox jumps over the lazy dog. Elide none"
        font.bold: false
        elide: Text.ElideNone
        color: "black"
        font.family: "Arial"
        font.pointSize: 24
    }
    Text {
        id: text_0004
        width: 150
        anchors.top: text_0003.bottom
        anchors.left: text_0003.left
        text: "The quick brown fox jumps over the lazy dog. wrapMode: NoWrap"
        font.bold: false
        wrapMode: Text.NoWrap
        color: "green"
        font.family: "Arial"
        font.pointSize: 24
    }
    Text {
        id: text_0005
        width: 150
        anchors.top: text_0004.bottom
        anchors.left: text_0004.left
        text: "The quick brown fox jumps over the lazy dog. wrapMode: WordWrap "
        font.bold: false
        wrapMode: Text.WordWrap
        color: "light grey"
        font.family: "Arial"
        font.pointSize: 12
    }
    Text {
        id: text_0006
        width: 150
        anchors.top: text_0005.bottom
        anchors.left: text_0005.left
        text: "The quick brown fox jumps over the lazy dog. wrapMode: wrapAnywhere "
        font.bold: false
        wrapMode: Text.WrapAnywhere
        color: "black"
        font.family: "Arial"
        font.pointSize: 12
    }
    Text {
        id: text_0007
        width: 150
        anchors.top: text_0006.bottom
        anchors.left: text_0006.left
        text: "The quick brown fox jumps over the lazy dog. wrapMode: wrap "
        font.bold: false
        wrapMode: Text.Wrap
        color: "light green"
        font.family: "Arial"
        font.pointSize: 12
    }
}
