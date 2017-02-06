import Qt.test 1.0
import QtQuick 2.0

QtObject {
    property ImplementedExtensionObject a : ImplementedExtensionObject { id: root; extendedProperty : 42 }
    property int b: root.abstractProperty
    property int c: root.implementedProperty
    property int d: root.extendedProperty

    function getAbstractProperty() { return b; }
    function getImplementedProperty() { return c; }
    function getExtendedProperty() { return d; }
}
