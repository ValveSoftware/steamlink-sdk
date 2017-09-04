import QtQml 2.0

QtObject {
    property int width: 400
    property int height: 400

    onWidthChanged: console.log(width);
    Component.onCompleted: width = 500;
}
