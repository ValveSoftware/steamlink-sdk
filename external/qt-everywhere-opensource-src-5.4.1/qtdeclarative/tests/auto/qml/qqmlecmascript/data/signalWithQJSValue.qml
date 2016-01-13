import Qt.test 1.0

MyQmlObject {
    property string expression
    property string compare
    property bool pass: false

    onSignalWithQJSValue:
    {
        qjsvalueMethod(arg);
        var expected = eval(expression);
        pass = eval(compare)(arg, expected);
    }
}
