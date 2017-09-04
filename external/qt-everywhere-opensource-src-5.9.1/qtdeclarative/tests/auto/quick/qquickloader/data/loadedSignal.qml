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
        // we set active to true, which triggers loading.
        // we then immediately set active to false.
        // this should clear the incubator and stop loading.
        loader.active = true;
        loader.active = false;
    }

    function activate() {
        loader.active = true;
    }

    function deactivate() {
        loader.active = false;
    }

    function triggerMultipleLoad() {
        loader.active = false;          // deactivate as a precondition.
        loader.source = "BlueRect.qml"
        loader.active = true;           // should trigger loading to begin
        loader.source = "RedRect.qml";  // should clear the incubator and restart loading
    }
}
