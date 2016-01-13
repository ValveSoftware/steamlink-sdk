import QtQuick 1.0

Rectangle {
    width: 400
    height: 400

    function setLoaderSource() {
        myLoader.source = "GreenRect.qml"
    }

    Loader {
        id: myLoader
    }
}
