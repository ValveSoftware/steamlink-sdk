import QtQuick 2.0

QtObject {
    property int test1: (testData1 * 32 + 99) / testData2
    property int test2: myValueFunction()

    property bool myValueFunctionCalled: false

    property int testData1: 19
    property int testData2: 13

    function myValueFunction() {
        myValueFunctionCalled = true;
        return 13;
    }
}

