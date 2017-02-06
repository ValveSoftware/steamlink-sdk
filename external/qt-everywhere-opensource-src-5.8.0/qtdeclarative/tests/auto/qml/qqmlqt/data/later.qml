import QtQuick 2.0
import LaterImports 1.0

Row {
    id: row
    Repeater {
        id: repeater
        model: 5
        delegate: Item { }
    }

    property bool test1_1: false
    property bool test1_2: row.focus
    property bool test2_1: false
    property bool test2_2: (firstFunctionCallCounter == 1 && secondFunctionCallCounter == 1 && signalCallCounter == 1)

    property int firstFunctionCallCounter: 0
    property int secondFunctionCallCounter: 0
    property int signalCallCounter: 0

    signal testSignal
    onTestSignal: {
        signalCallCounter++;
    }

    onChildrenChanged: {
        Qt.callLater(row.forceActiveFocus); // built-in function
        Qt.callLater(row.firstFunction);    // JS function
        Qt.callLater(row.testSignal);       // signal
    }

    function firstFunction() {
        firstFunctionCallCounter++;
    }

    function secondFunction() {
        secondFunctionCallCounter++;
    }

    Component.onCompleted: {
        test1_1 = !row.focus;
        test2_1 = (firstFunctionCallCounter == 0);

        Qt.callLater(secondFunction);
        Qt.callLater(firstFunction);
        Qt.callLater(secondFunction);
    }

    function test2() {
        repeater.model = 2;
    }

    function test3() {
        Qt.callLater(test3_recursive);
    }

    property int recursion: 0
    property bool test3_1: (recursion == 1)
    property bool test3_2: (recursion == 2)
    property bool test3_3: (recursion == 3)
    function test3_recursive() {
        if (recursion < 3) {
            Qt.callLater(test3_recursive);
            Qt.callLater(test3_recursive);
        }
        recursion++;
    }

    function test4() {
        Qt.callLater(functionThatDoesNotExist);
    }

    property bool test5_1: false
    function test5() {
        Qt.callLater(functionWithArguments, "THESE", "ARGS", "WILL", "BE", "OVERWRITTEN")
        Qt.callLater(functionWithArguments, "firstArg", 2, "thirdArg")
    }

    function functionWithArguments(firstStr, secondInt, thirdString) {
        test5_1 = (firstStr == "firstArg" && secondInt == 2 && thirdString == "thirdArg");
    }


    property bool test6_1: (callOrder_Later == "TWO THREE ") // not "THREE TWO "
    function test6() {
        Qt.callLater(test6Function1, "ONE");
        Qt.callLater(test6Function2, "TWO");
        Qt.callLater(test6Function1, "THREE");
    }

    property string callOrder_Later
    function test6Function1(arg) { callOrder_Later += arg + " "; }
    function test6Function2(arg) { callOrder_Later += arg + " "; }

    property int test9_1: SingletonType.intProp;
    function test9() {
        SingletonType.resetIntProp();
        Qt.callLater(SingletonType.testFunc)
        Qt.callLater(SingletonType.testFunc)
        Qt.callLater(SingletonType.testFunc)
        Qt.callLater(SingletonType.testFunc)
        // should only get called once.
    }

    property int test10_1: 0
    function test10() {
        var c = Qt.createComponent("LaterComponent.qml");
        var obj = c.createObject(); // QML ownership.
        Qt.callLater(obj.testFn);
        // note: obj will be cleaned up during next gc().
    }

    property int test11_1: 0
    function test11() {
        var c = Qt.createComponent("LaterComponent2.qml");
        var obj = c.createObject(); // QML ownership.
        Qt.callLater(obj.testFn);
        gc(); // this won't actually collect the obj, we need to trigger gc manually in the test.
    }

    function test14() {
        Qt.callLater(console.log, "success")
    }
}
