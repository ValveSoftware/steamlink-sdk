import QtQuick 2.0

QtObject {
    id: root

    property QtObject objectProperty

    property Component c: Component {
        id: componentObject
        QtObject {
        }
    }

    function create() {
        objectProperty = c.createObject(root);
    }

    function destroy() {
        objectProperty.destroy();
    }
}
