import Test 1.0
import QtQuick 2.0

MyTypeObject {
    flagProperty: "FlagVal1 | FlagVal3"
    enumProperty: "EnumVal2"
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
    vector4Property: "10,1,2.2,2.3"
    urlProperty: "main.qml?with%3cencoded%3edata"

    objectProperty: MyTypeObject {}

    property var varProperty: "Hello World!"

    property list<MyQmlObject> resources: [
        MyQmlObject { objectName: "flagProperty"; qjsvalue: flagProperty },
        MyQmlObject { objectName: "enumProperty"; qjsvalue: enumProperty },
        MyQmlObject { objectName: "stringProperty"; qjsvalue: stringProperty },
        MyQmlObject { objectName: "uintProperty"; qjsvalue: uintProperty },
        MyQmlObject { objectName: "intProperty"; qjsvalue: intProperty },
        MyQmlObject { objectName: "realProperty"; qjsvalue: realProperty },
        MyQmlObject { objectName: "doubleProperty"; qjsvalue: doubleProperty },
        MyQmlObject { objectName: "floatProperty"; qjsvalue: floatProperty },
        MyQmlObject { objectName: "colorProperty"; qjsvalue: colorProperty },
        MyQmlObject { objectName: "dateProperty"; qjsvalue: dateProperty },
        MyQmlObject { objectName: "timeProperty"; qjsvalue: timeProperty },
        MyQmlObject { objectName: "dateTimeProperty"; qjsvalue: dateTimeProperty },
        MyQmlObject { objectName: "pointProperty"; qjsvalue: pointProperty },
        MyQmlObject { objectName: "pointFProperty"; qjsvalue: pointFProperty },
        MyQmlObject { objectName: "sizeProperty"; qjsvalue: sizeProperty },
        MyQmlObject { objectName: "sizeFProperty"; qjsvalue: sizeFProperty },
        MyQmlObject { objectName: "rectProperty"; qjsvalue: rectProperty },
        MyQmlObject { objectName: "rectFProperty"; qjsvalue: rectFProperty },
        MyQmlObject { objectName: "boolProperty"; qjsvalue: boolProperty },
        MyQmlObject { objectName: "variantProperty"; qjsvalue: variantProperty },
        MyQmlObject { objectName: "vectorProperty"; qjsvalue: vectorProperty },
        MyQmlObject { objectName: "vector4Property"; qjsvalue: vector4Property },
        MyQmlObject { objectName: "urlProperty"; qjsvalue: urlProperty },
        MyQmlObject { objectName: "objectProperty"; qjsvalue: objectProperty },
        MyQmlObject { objectName: "varProperty"; qjsvalue: varProperty }
    ]
}
