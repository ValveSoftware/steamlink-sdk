import Qt.test 1.0

TestObject {
    id: root

    function runtest() {
        var r = root;

        for (var ii = 0; ii < 5000000; ++ii) {
            r.stringValue
        }
    }
}

