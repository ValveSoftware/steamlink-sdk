import QtQuick 2.0
import Qt.test 1.0

Item {
    MyQmlObject {
        value: if (1) 3
    }

    MyQmlObject {
        value: { deleteMe(), 2 }
    }
}
