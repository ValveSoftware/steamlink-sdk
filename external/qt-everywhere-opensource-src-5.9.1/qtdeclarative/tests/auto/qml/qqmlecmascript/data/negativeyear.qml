import QtQuick 2.0
import Qt.test 1.0

Item {
    function check_negative_tostring() {
        return "result: " + new Date(-2000, 0, 1);
    }

    function check_negative_toisostring() {
        // Make that february, to avoid timezone problems
        return "result: " + (new Date(-2000, 1, 1)).toISOString();
    }
}
