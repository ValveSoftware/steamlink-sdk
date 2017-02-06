import Test 1.0
import QtQuick 2.0

MyTypeObject {
    flagProperty: "FlagVal1 | FlagVal3"
    enumProperty: "EnumVal2"
    qtEnumProperty: Qt.RichText
    mirroredEnumProperty: Qt.AlignHCenter
    relatedEnumProperty: "RelatedValue"
    stringProperty: "Hello World!"
    uintProperty: 10
    intProperty: -19
    realProperty: 23.2
    doubleProperty: -19.7
    floatProperty: 8.5
    colorProperty: "red"
    dateProperty: "1982-11-25"
    timeProperty: "11:11:32"
    dateTimeProperty: "2009-05-12T13:22:01"
    pointProperty: "99,13"
    pointFProperty: "-10.1,12.3"
    sizeProperty: "99x13"
    sizeFProperty: "0.1x0.2"
    rectProperty: "9,7,100x200"
    rectFProperty: "1000.1,-10.9,400x90.99"
    boolProperty: true
    variantProperty: "Hello World!"
    vectorProperty: "10,1,2.2"
    vector2Property: "2, 3"
    vector4Property: "10,1,2.2,2.3"
    quaternionProperty: "4,5,6,7"
    urlProperty: "main.qml?with%3cencoded%3edata"

    objectProperty: MyTypeObject { intProperty: 8 }

    property bool qtEnumTriggeredChange: false
    onQtEnumPropertyChanged: qtEnumTriggeredChange = true

    property bool mirroredEnumTriggeredChange: false
    onMirroredEnumPropertyChanged: mirroredEnumTriggeredChange = true
}
