import QtQuick 2.0

Rectangle { width: 300; height: 300; color: "white"
    resources: [
        Component { id:cursorFail; FailItem { objectName: "delegateFail" } },
        Component { id:cursorWait; WaitItem { objectName: "delegateSlow" } },
        Component { id:cursorNorm; NormItem { objectName: "delegateOkay" } }
    ]
    TextEdit {
        cursorDelegate: cursorFail
        cursorVisible: true
    }
    TextEdit {
        cursorDelegate: cursorWait
        cursorVisible: true
    }
    TextEdit {
        cursorDelegate: cursorNorm
        cursorVisible: true
    }
}
