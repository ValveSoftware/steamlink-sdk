import Qt.test 1.0
import QtQuick 2.0

QtObject {
    property AbstractExtensionObject a;
    a: AbstractExtensionObject { id: root }
    property int b: Math.max(root.abstractProperty, 0)
}
