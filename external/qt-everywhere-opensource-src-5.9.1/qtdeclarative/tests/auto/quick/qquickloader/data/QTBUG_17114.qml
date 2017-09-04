import QtQuick 2.0

Rectangle {
    property real loaderWidth: loader.width
    property real loaderHeight: loader.height
    width: 200
    height: 200

    Loader {
        id: loader
        sourceComponent: Item {
            property real iwidth: 32
            property real iheight: 32
            width: iwidth
            height: iheight
        }
    }
}
