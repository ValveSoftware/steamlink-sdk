// Benchmarks the cost of accessing an id in the same file as the script.

import QtQuick 2.0

QtObject {
    id: root

    function runtest() {
        for (var ii = 0; ii < 5000000; ++ii) {
            root
        }
    }
}
