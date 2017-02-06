import QtQuick 2.0

Rectangle {
    width: 300
    height: 300
    color: "white"

    TextEdit {
        objectName: "textEditObject"
        width: 300
        height: 300
        text: "<span style=\"font-size:20pt;\">Blah</span><br>blah"
        textFormat: TextEdit.RichText
        cursorDelegate: Rectangle {
            objectName: "cursorInstance"
            color: "red"
            width: 2
        }
    }
}
