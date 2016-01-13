import QtQuick 2.0

QtObject {
    property QtObject o: AsynchronousIfNestedType { }
    property int dummy: 11
}
