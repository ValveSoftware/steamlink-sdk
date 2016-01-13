import QtQuick 1.0

Rectangle {
    width: 100; height: 100; color: "red"
    signal somethingHappened
    onSomethingHappened: QtObject {}
}
