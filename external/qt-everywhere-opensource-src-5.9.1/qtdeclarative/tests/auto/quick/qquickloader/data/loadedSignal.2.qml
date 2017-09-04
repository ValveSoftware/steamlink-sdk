import QtQuick 2.0

Item {
    id: root

    width: 200
    height: 200

    property bool success: true
    property int loadCount: 0

    Loader {
        id: loader
        anchors.fill: parent
        asynchronous: true
        active: false
        source: "TestComponent.qml"
        onLoaded: {
            if (status !== Loader.Ready) {
                root.success = false;
            }
            root.loadCount++;
        }
    }

    function triggerLoading() {
        // we set source to a valid path (but which is an invalid / erroneous component)
        // we should not get onLoaded, since the status should not be Ready.
        loader.source = "GreenRect.qml" // causes reference error.
    }
}
