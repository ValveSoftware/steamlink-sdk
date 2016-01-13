import QtQuick 2.0

Rectangle { width: 300; height: 300; color: "white"
    TextInput {
        text: "Hello world!"
        id: textInputObject;
        objectName: "textInputObject"
        width: 300; height: 300
        wrapMode: TextInput.Wrap
        cursorDelegate: contextDelegate
    }
}
