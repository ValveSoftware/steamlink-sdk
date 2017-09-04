import QtQuick 2.3

Rectangle {
    width: 360
    height: 360

    property alias loaderSource: loader.source

    Component {
        id: crashComponent

        Item {
            Image {
                transform: scale
            }

            Scale {
                id: scale
                xScale: 1
                yScale: 1
            }
        }
    }

    Loader {
        id: loader
        anchors.fill: parent
        sourceComponent: crashComponent
    }
}

