import Qt.test 1.0
import QtQuick 2.0

QtObject {
    id: root

    property int count: 0
    signal testSignal
    onTestSignal: count++

    property int funcCount: 0
    function testFunction() {
        funcCount++;
    }

    //should increment count
    function testSignalCall() {
        testSignal()
    }

    //should NOT increment count, and should throw an exception
    property string errorString
    function testSignalHandlerCall() {
        try {
            onTestSignal()
        } catch (error) {
            errorString = error.toString();
        }
    }

    //should increment funcCount once
    function testSignalConnection() {
        testSignal.connect(testFunction)
        testSignal();
        testSignal.disconnect(testFunction)
        testSignal();
    }

    //should increment funcCount once
    function testSignalHandlerConnection() {
        onTestSignal.connect(testFunction)
        testSignal();
        onTestSignal.disconnect(testFunction)
        testSignal();
    }

    //should be defined
    property bool definedResult: false
    function testSignalDefined() {
        if (testSignal !== undefined)
            definedResult = true;
    }

    //should be defined
    property bool definedHandlerResult: false
    function testSignalHandlerDefined() {
        if (onTestSignal !== undefined)
            definedHandlerResult = true;
    }

    property QtObject objWithAlias: QtObject {
        id: testObjectWithAlias

        property int count: 0;
        property alias countAlias: testObjectWithAlias.count
    }

    function testConnectionOnAlias() {
        var called = false;

        testObjectWithAlias.onCountAliasChanged.connect(function() {
            called = true
        })

        testObjectWithAlias.count++;
        return called;
    }

    property QtObject objWithAliasHandler: QtObject {
        id: testObjectWithAliasHandler

        property bool testSuccess: false

        property int count: 0
        property alias countAlias: testObjectWithAliasHandler.count
        onCountAliasChanged: testSuccess = true
    }

    function testAliasSignalHandler() {
        testObjectWithAliasHandler.testSuccess = false
        testObjectWithAliasHandler.count++
        return testObjectWithAliasHandler.testSuccess
    }

    signal signalWithClosureArgument(var f)
    onSignalWithClosureArgument: f()

    function testSignalWithClosureArgument() {
        var testSuccess = false
        signalWithClosureArgument(function() {
            testSuccess = true
        })
        return testSuccess
    }
}
