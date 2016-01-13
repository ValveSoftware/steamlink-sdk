import Qt.test 1.0

WriteCounter {
    property int x: 0
    value: if (1) x + x
}
