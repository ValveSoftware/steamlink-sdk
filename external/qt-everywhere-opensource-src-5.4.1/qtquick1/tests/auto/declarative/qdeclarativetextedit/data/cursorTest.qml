import QtQuick 1.0

Rectangle { width: 300; height: 300; color: "white"
    TextEdit {  text: "Hello world!"; id: textEditObject; objectName: "textEditObject"
        anchors.fill: parent
        resources: [ Component { id:cursor; Item { id:cursorInstance; objectName: "cursorInstance" } } ]
        cursorDelegate: cursor
    }
}
