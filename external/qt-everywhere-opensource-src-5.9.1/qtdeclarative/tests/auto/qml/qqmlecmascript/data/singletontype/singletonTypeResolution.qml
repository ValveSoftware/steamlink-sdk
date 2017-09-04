import QtQuick 2.0
import Qt.test.importOrderApi 1.0
import Qt.test.importOrderApi 1.0 as Namespace
import NamespaceAndType 1.0
import NamespaceAndType 1.0 as NamespaceAndType

QtObject {
    property bool success: false

    Component.onCompleted: {
        var s0 = Data.value === 37 && Namespace.Data.value === 37 && Data.value === Namespace.Data.value;
        var s1 = NamespaceAndType.NamespaceAndType.value === 37; // qualifier should shadow typename.
        var s2 = NamespaceAndType.value === undefined; // should resolve to the qualifier, not the singleton type.
        success = (s0 === true) && (s1 === true) && (s2 === true);
    }
}
