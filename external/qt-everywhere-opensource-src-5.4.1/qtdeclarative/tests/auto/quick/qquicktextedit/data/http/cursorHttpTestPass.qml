import QtQuick 2.0

Rectangle { width: 300; height: 300; color: "white"
    resources: [
        Component { id:cursorWait; WaitItem { objectName: "delegateSlow" } },
        Component { id:cursorNorm; NormItem { objectName: "delegateOkay" } }
    ]
    TextEdit {
        cursorDelegate: cursorWait
        text: "Hello"
        cursorVisible: true
    }
    TextEdit {
        objectName: "textEditObject"
        cursorDelegate: cursorNorm
        cursorVisible: true
        focus: true;
        text: "Hello"
    }
}
