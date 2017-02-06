import QtQuick 2.0
import Qt.test 1.0

Item {
    SelfRegisteringOuter {
        value: SelfRegistering {
            value: 11
        }
    }
}
