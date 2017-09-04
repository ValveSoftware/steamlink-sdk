import QtQuick 2.0

Text {
    width: implicitWidth / 2
    height: implicitHeight / 2
    elide: Text.ElideRight
    maximumLineCount: 4

    text: "Line one\nLine two\nLine three\nLine four"
}
