import QtQuick 1.0

QtObject {
    objectName: "hello"

    property string test1: objectName
    property string test2: objectName.substr(1, 3)
}
