import QtQuick 2.0

Rectangle {
    width: 640
    height: 480
    Image {
        id: image
        width: 40
        source: "image.png"
    }

    states: State {
        name: "state1"
        PropertyChanges {
            target: image
            width: undefined
        }
    }
}
