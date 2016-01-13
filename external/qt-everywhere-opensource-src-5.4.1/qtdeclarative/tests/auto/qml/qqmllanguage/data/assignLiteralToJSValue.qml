// This tests assigning literals to QJSValue properties.
// These properties store JavaScript object references.

import QtQuick 2.0
import Test 1.0

QtObject {
    property list<QtObject> resources: [
        MyQmlObject { id: testObj1; objectName: "test1"; qjsvalue: 1 },
        MyQmlObject { id: testObj2; objectName: "test2"; qjsvalue: 1.7 },
        MyQmlObject { id: testObj3; objectName: "test3"; qjsvalue: "Hello world!" },
        MyQmlObject { id: testObj4; objectName: "test4"; qjsvalue: "#FF008800" },
        MyQmlObject { id: testObj5; objectName: "test5"; qjsvalue: "10,10,10x10" },
        MyQmlObject { id: testObj6; objectName: "test6"; qjsvalue: "10,10" },
        MyQmlObject { id: testObj7; objectName: "test7"; qjsvalue: "10x10" },
        MyQmlObject { id: testObj8; objectName: "test8"; qjsvalue: "100,100,100" },
        MyQmlObject { id: testObj9; objectName: "test9"; qjsvalue: String("#FF008800") },
        MyQmlObject { id: testObj10; objectName: "test10"; qjsvalue: true },
        MyQmlObject { id: testObj11; objectName: "test11"; qjsvalue: false },
        MyQmlObject { id: testObj12; objectName: "test12"; qjsvalue: Qt.rgba(0.2, 0.3, 0.4, 0.5) },
        MyQmlObject { id: testObj13; objectName: "test13"; qjsvalue: Qt.rect(10, 10, 10, 10) },
        MyQmlObject { id: testObj14; objectName: "test14"; qjsvalue: Qt.point(10, 10) },
        MyQmlObject { id: testObj15; objectName: "test15"; qjsvalue: Qt.size(10, 10) },
        MyQmlObject { id: testObj16; objectName: "test16"; qjsvalue: Qt.vector3d(100, 100, 100); },
        MyQmlObject { id: testObj17; objectName: "test17"; qjsvalue: "1" },
        MyQmlObject { id: testObj18; objectName: "test18"; qjsvalue: "1.7" },
        MyQmlObject { id: testObj19; objectName: "test19"; qjsvalue: "true" },
        MyQmlObject { id: testObj20; objectName: "test20"; qjsvalue: function(val) { return val * 3; } },
        MyQmlObject { id: testObj21; objectName: "test21"; qjsvalue: undefined },
        MyQmlObject { id: testObj22; objectName: "test22"; qjsvalue: null },
        MyQmlObject { id: testObj1Bound; objectName: "test1Bound"; qjsvalue: testObj1.qjsvalue + 4 },  // 1 + 4 + 4 = 9
        MyQmlObject { id: testObj20Bound; objectName: "test20Bound"; qjsvalue: testObj20.qjsvalue(testObj1Bound.qjsvalue) }, // 9 * 3 = 27
        QtObject {
            id: varProperties
            objectName: "varProperties"
            property var test1: testObj1.qjsvalue
            property var test2: testObj2.qjsvalue
            property var test3: testObj3.qjsvalue
            property var test4: testObj4.qjsvalue
            property var test5: testObj5.qjsvalue
            property var test6: testObj6.qjsvalue
            property var test7: testObj7.qjsvalue
            property var test8: testObj8.qjsvalue
            property var test9: testObj9.qjsvalue
            property var test10: testObj10.qjsvalue
            property var test11: testObj11.qjsvalue
            property var test12: testObj12.qjsvalue
            property var test13: testObj13.qjsvalue
            property var test14: testObj14.qjsvalue
            property var test15: testObj15.qjsvalue
            property var test16: testObj16.qjsvalue
            property var test20: testObj20.qjsvalue

            property var test1Bound: testObj1.qjsvalue + 4 // 1 + 4 + 4 = 9
            property var test20Bound: testObj20.qjsvalue(test1Bound) // 9 * 3 = 27
        },
        QtObject {
            id: variantProperties
            objectName: "variantProperties"
            property variant test1: testObj1.qjsvalue
            property variant test2: testObj2.qjsvalue
            property variant test3: testObj3.qjsvalue
            property variant test4: testObj4.qjsvalue
            property variant test5: testObj5.qjsvalue
            property variant test6: testObj6.qjsvalue
            property variant test7: testObj7.qjsvalue
            property variant test8: testObj8.qjsvalue
            property variant test9: testObj9.qjsvalue
            property variant test10: testObj10.qjsvalue
            property variant test11: testObj11.qjsvalue
            property variant test12: testObj12.qjsvalue
            property variant test13: testObj13.qjsvalue
            property variant test14: testObj14.qjsvalue
            property variant test15: testObj15.qjsvalue
            property variant test16: testObj16.qjsvalue

            property variant test1Bound: testObj1.qjsvalue + 4 // 1 + 4 + 4 = 9
            property variant test20Bound: testObj20.qjsvalue(test1Bound) // 9 * 3 = 27
        },
        MyTypeObject {
            objectName: "typedProperties"
            intProperty: testObj1.qjsvalue
            doubleProperty: testObj2.qjsvalue
            stringProperty: testObj3.qjsvalue
            boolProperty: testObj10.qjsvalue
            colorProperty: testObj12.qjsvalue
            rectFProperty: testObj13.qjsvalue
            pointFProperty: testObj14.qjsvalue
            sizeFProperty: testObj15.qjsvalue
            vectorProperty: testObj16.qjsvalue
        },
        MyTypeObject {
            objectName: "stringProperties"
            intProperty: testObj17.qjsvalue
            doubleProperty: testObj18.qjsvalue
            stringProperty: testObj3.qjsvalue
            boolProperty: testObj19.qjsvalue
            colorProperty: testObj4.qjsvalue
            rectFProperty: testObj5.qjsvalue
            pointFProperty: testObj6.qjsvalue
            sizeFProperty: testObj7.qjsvalue
            vectorProperty: testObj8.qjsvalue
        }
    ]

    Component.onCompleted: {
        testObj1.qjsvalue = testObj1.qjsvalue + 4
    }
}
