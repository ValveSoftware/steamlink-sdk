import QtQuick 2.0

Rectangle {
    width: 400; height: 400

    property bool showRect: false
    onShowRectChanged: if (showRect) rect.visible = true
    property bool noFocus: !fs2.activeFocus

    FocusScope {
        id: fs1
        focus: true
    }
    Rectangle {
        id: rect
        visible: false
        FocusScope {
            id: fs2
            Rectangle {
                focus: true
            }
        }
    }
}
