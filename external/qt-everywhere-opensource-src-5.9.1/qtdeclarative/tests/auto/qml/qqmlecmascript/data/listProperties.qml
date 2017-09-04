import Qt.test 1.0
import QtQuick 2.0

MyQmlObject {
    id: root

    objectListProperty: [
        QtObject { property int a: 10 },
        QtObject { property int a: 11 }
    ]

    function calcTest1() {
        var rv = 0;
        for (var ii = 0; ii < root.objectListProperty.length; ++ii) {
            rv += root.objectListProperty[ii].a;
        }
        return rv;
    }

    property int test1: calcTest1();
    property int test2: root.objectListProperty.length
    property bool test3: root.objectListProperty[1] != undefined
    property bool test4: root.objectListProperty[100] == undefined
}
