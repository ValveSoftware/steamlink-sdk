import Qt.test 1.0

MyQmlObject {
    property var other: MyQmlObject {
        intProperty: 123

        function go() {
            return intProperty;
        }
    }

    value: with(other) go()
}

