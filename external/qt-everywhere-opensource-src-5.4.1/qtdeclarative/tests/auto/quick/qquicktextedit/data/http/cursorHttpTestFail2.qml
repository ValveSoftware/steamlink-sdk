import QtQuick 2.0

Rectangle { width: 300; height: 300; color: "white"
    resources: [
        Component { id:cursorWait; WaitItem { objectName: "delegateSlow" } },
        Component { id:cursorNorm; NormItem { objectName: "delegateOkay" } },
        Component { id:cursorErr; ErrItem { objectName: "delegateErrorA" } }
    ]
    TextEdit {
        cursorDelegate: cursorWait
        cursorVisible: true
    }
    TextEdit {
        cursorDelegate: cursorNorm
        cursorVisible: true
    }
    TextEdit {
        cursorDelegate: cursorErr
        cursorVisible: true
    }
}
