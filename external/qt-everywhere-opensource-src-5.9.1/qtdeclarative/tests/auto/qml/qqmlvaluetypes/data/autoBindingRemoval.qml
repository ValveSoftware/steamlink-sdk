import Test 1.0

MyTypeObject {
    property int value: 10
    rect.x: value

    onRunScript: { rect.x = 42; }
}

