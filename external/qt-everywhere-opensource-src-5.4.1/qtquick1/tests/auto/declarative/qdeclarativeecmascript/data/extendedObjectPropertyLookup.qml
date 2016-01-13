import Qt.test 1.0
import QtQuick 1.0

QtObject {
    property MyExtendedObject a;
    a: MyExtendedObject { id: root }
    property int b: Math.max(root.extendedProperty, 0)
}
