import QtQuick 2.0

Rectangle { width: 300; height: 300; color: "white"
    property string contextualProperty: "Hello"
    TextEdit {
        text: "Hello world!"
        id: textEditObject
        wrapMode: TextEdit.Wrap
        objectName: "textEditObject"
        width: 300; height: 300
        cursorDelegate: Item {
            id:cursorInstance
            objectName: "cursorInstance"
            property string localProperty: contextualProperty
        }
    }
}
