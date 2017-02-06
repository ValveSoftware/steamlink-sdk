import QtQuick 2.0

Item {
    width: 550
    height: 480

    TextEdit {
        id: textEdit
        anchors.centerIn: parent
        width: 550
        textFormat: TextEdit.RichText
        text: "A<br />
    This is a long message to demenostrate a text selection issue. I need to type some more text here. This is line 3<br />
    This is a long message to demenostrate a text selection issue. I need to type some more text here. This is line 4"
        wrapMode: TextEdit.Wrap

        Component.onCompleted: {
            textEdit.selectAll()
        }
    }

}
