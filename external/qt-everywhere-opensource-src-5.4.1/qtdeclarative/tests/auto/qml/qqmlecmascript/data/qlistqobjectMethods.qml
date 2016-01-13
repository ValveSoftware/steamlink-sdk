import QtQuick 2.0

QtObject {
    property int test: getObjects().length
    property bool test2: getObjects()[0].trueProperty
}
