import QtQuick 2.0

Item {
    id: root
    property bool success: false

    ListModel {
        id: theModel
        ListElement {
            name: "Apple"
        }
    }

    property var firstDelegate;

    Repeater {
        id: repeater
        model: theModel

        delegate: Item {
            property var myVarProperty: name;
            Component.onCompleted: firstDelegate = this;
        }
    }

    Component.onCompleted: {
        repeater.model = null;
        firstDelegate.myVarProperty = 42;
        success = (firstDelegate.myVarProperty === 42);
    }
}
