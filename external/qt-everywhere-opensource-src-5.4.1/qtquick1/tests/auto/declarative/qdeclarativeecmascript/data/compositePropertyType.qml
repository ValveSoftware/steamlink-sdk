import QtQuick 1.0

QtObject {
    property CustomObject myObject
    myObject: CustomObject { }

    Component.onCompleted: console.log(myObject.greeting)
}
