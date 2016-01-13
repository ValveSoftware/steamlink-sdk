import QtQuick 2.0
import "SpecificComponent"

Item {
    id: root
    property SpecificComponent first
    property SpecificComponent second

    property bool success: false

    Component.onCompleted: {
        var c1 = Qt.createComponent("./SpecificComponent/SpecificComponent.qml");
        var o1 = c1.createObject(root);
        first = o1;

        var c2 = Qt.createComponent("./OtherComponent/OtherComponent.qml");
        var o2 = c2.createObject(root);
        second = o2.sc;

        var ft = first.toString().substr(0, first.toString().indexOf('('));
        var st = second.toString().substr(0, second.toString().indexOf('('));
        if (ft == st) success = true;
    }
}
