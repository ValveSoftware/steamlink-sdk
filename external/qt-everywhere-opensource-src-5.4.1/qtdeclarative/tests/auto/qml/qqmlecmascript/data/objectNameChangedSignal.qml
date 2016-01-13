import QtQuick 2.0

QtObject {
    id: root
    property bool test: false

    onObjectNameChanged: test = true
}
