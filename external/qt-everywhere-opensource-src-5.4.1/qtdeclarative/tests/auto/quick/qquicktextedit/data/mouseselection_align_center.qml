import QtQuick 2.0

TextEdit {
    focus: true
    text:
"0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ
9876543210

ZXYWVUTSRQPON MLKJIHGFEDCBA"
    width: implicitWidth - 10
    selectByMouse: true
    horizontalAlignment: TextEdit.AlignHCenter
    verticalAlignment: TextEdit.AlignVCenter
}
