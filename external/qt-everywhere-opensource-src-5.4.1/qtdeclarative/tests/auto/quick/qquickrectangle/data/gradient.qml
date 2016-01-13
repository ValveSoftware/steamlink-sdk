import QtQuick 2.0

Rectangle {

    function resetGradient() {
        gradient = undefined
    }

    gradient: Gradient {
        GradientStop { position: 0.0; color: "gray" }
        GradientStop { position: 1.0; color: "white" }
    }
}

