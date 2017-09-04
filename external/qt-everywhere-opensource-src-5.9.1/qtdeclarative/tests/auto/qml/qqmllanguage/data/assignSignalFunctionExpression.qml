import Test 1.0
MyQmlObject {
    onBasicSignal: function() { basicSlot() }
    onBasicParameterizedSignal: function(param) { basicSlotWithArgs(param) }
}
