import QtQuick 2.0
Rectangle {
    width: 400
    height: 400

    color.r: 1
    color.g: 0
    color.b: 1

    Behavior on color.r { NumberAnimation { duration: 500; } }

    function changeR() { color.r = 0 }
}
