import QtQuick 2.0

Rectangle {
    width: 400; height: 400

    property string comp
    function loadComponent() {
        loader.source = comp
    }

    Loader {
        id: loader
        objectName: "loader"
        asynchronous: true
    }
}
