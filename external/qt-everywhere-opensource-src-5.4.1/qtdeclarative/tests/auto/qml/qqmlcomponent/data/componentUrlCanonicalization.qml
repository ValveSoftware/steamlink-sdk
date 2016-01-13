import QtQuick 2.0
import "SpecificComponent"
import "OtherComponent"

Item {
    id: root
    property SpecificComponent first
    property SpecificComponent second
    property OtherComponent oc: OtherComponent { }

    property bool success: false

    Component.onCompleted: {
        var c1 = Qt.createComponent("./SpecificComponent/SpecificComponent.qml");
        var o1 = c1.createObject(root);
        first = o1;
        second = oc.sc;

        // We want to ensure that the types are the same, ie, that the
        // component hasn't been registered twice due to failed
        // canonicalization of the component path when importing.
        // The type is reported in the toString() output prior to the
        // instance pointer value.

        // in our case, the type string should be:
        // SpecificComponent_QMLTYPE_0
        var ft = first.toString().substr(0, first.toString().indexOf('('));
        var st = second.toString().substr(0, second.toString().indexOf('('));
        if (ft == st) success = true;
    }
}
