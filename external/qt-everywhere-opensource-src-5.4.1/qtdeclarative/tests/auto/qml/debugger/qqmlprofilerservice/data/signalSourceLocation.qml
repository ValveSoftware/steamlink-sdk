import QtQuick 2.0

Rectangle {
    width: 400
    height: 400

    onWidthChanged: console.log(width);
    Component.onCompleted: width = 500;
}
