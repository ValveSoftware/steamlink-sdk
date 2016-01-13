import QtQuick 1.1
import "../shared" 1.0

TextInput {
    id: inp
    FontLoader { id: fixedFont; source: "DejaVuSansMono.ttf" }
    font.family: fixedFont.name
    font.pixelSize: 12
    cursorDelegate: Rectangle {
            width: 1;
            color: "black";
            visible: parent.cursorVisible//bug that 'inp' doesn't seem to work?
    }
}
