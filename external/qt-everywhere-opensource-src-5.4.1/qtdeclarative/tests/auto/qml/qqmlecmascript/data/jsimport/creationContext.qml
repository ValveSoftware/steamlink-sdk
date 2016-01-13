import QtQuick 2.0
import "importFour.js" as SomeScript

Item {
    id: root
    property bool success: false;
    Component {
        id: testComponent
        Item {
            property string valueFromScript: SomeScript.greetingString()
        }
    }
    property Loader loader;
    signal loaded
    onLoaded: {
        success = (loader.item.valueFromScript === SomeScript.greetingString())
    }
    Component.onCompleted: {
        loader = Qt.createQmlObject("import QtQuick 2.0; Loader {}", this, "dynamic loader")
        loader.onLoaded.connect(loaded)
        loader.sourceComponent = testComponent
    }
}

