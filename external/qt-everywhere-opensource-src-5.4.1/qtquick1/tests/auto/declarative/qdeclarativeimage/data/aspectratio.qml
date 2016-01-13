import QtQuick 1.0

Image {
    property int widthChange: 0
    property int heightChange: 0
    source: "heart.png"
    fillMode: Image.PreserveAspectFit;
    onWidthChanged: widthChange += 1
    onHeightChanged: heightChange += 1
}
