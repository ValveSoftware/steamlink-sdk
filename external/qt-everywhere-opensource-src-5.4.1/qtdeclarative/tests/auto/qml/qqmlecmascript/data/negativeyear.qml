import QtQuick 2.0
import Qt.test 1.0

Item {
    function check_negative() {
        return "result: " + new Date(-2000, 0, 1);
    }
}
