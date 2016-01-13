import QtQuick 2.0

QtObject {
    property var string1: "a"
    property var string2: "a"
    property var comparison: string1.localeCompare(string2)
}
