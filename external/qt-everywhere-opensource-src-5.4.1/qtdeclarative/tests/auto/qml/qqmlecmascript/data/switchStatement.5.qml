import Qt.test 1.0

MyQmlObject {
    value: {
        var value = 0
        switch (stringProperty) {
        default:
            value = value + 1
        }
    }
}

