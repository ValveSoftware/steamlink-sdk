import QtQuick 2.0
import "include.js" as IncludeTest

QtObject {
    property int test0: 0
    property bool test1: false
    property bool test2: false
    property bool test2_1: false
    property bool test3: false
    property bool test3_1: false

    property int testValue: 99

    Component.onCompleted: {
        IncludeTest.go();
        test0 = IncludeTest.value
        test1 = IncludeTest.test1
        test2 = IncludeTest.test2
        test2_1 = IncludeTest.test2_1
        test3 = IncludeTest.test3
        test3_1 = IncludeTest.test3_1
    }
}
