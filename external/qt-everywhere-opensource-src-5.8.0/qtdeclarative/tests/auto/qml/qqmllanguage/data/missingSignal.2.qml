import Test 1.0
MyQmlObject {
    function dynamicMethod() {
        basicSlot();
    }

    // invalid: signal handler definition given for non-signal method.
    onDynamicMethod: {
        basicSlot();
    }
}
