import QtQuick 1.0

Rectangle {
    id: page
    color: "white"
    width: 520; height: 280

    BorderImage {
        x: 20; y: 20; width: 230; height: 240
        smooth: true
        source: "content/colors-stretch.sci"
    }
    BorderImage {
        x: 270; y: 20; width: 230; height: 240
        smooth: true
        source: "content/colors-round.sci"
    }
}
