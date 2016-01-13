import QtQuick 2.0
import Qt.test 1.0

Item {
    id: root
    property bool test: false
    property QtObject objectProperty

    onObjectPropertyChanged: {
        root.test = true;
    }

    property Component c: Component {
        id: dynamicComponent
        QtObject {
            id: dynamicObject
        }
    }

    function create() {
        root.objectProperty = root.c.createObject(root);
    }

    function destroy() {
        root.test = false; // reset test
        root.objectProperty.destroy(100);
        // in cpp, wait objectProperty deletion, inspect "test" and "objectProperty"
        // test should be true and value of objectProperty should be zero.
    }
}
