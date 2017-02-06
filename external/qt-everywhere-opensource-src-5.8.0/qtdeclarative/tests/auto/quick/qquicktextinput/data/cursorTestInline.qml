import QtQuick 2.0

Rectangle { width: 300; height: 300; color: "white"
    property string contextualProperty: "Hello"
    TextInput {
        text: "Hello world!"
        id: textInputObject
        objectName: "textInputObject"
        width: 300; height: 300
        wrapMode: TextInput.WordWrap
        cursorDelegate: Item {
            id:cursorInstance
            objectName: "cursorInstance"
            property string localProperty: contextualProperty
        }
    }
}
