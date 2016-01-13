import QtQuick 1.1
import "../shared" 1.0

TextEdit {
    id: edit
    FontLoader { id: fixedFont; source: "DejaVuSansMono.ttf" }
    font.family: fixedFont.name
    font.pixelSize: 12
    cursorDelegate: Rectangle {
            width: 1;
            color: "black";
            visible: edit.cursorVisible
    }
}
