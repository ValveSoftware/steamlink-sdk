import Test 1.0

MyTypeObject {
    property int value: 10
    rect.x: value

    onRunScript: { rect = Qt.rect(10, 10, 10, 10) }
}

