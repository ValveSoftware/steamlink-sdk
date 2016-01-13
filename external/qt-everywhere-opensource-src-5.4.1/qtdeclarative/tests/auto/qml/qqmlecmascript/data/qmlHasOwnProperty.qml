import QtQuick 2.0
import Qt.test 1.0
import Qt.test.qobjectApi 1.0 as QtTestQObjectApi

Item {
    id: obj
    objectName: "objName"
    property int someIntProperty: 10
    property bool result: false

    function testHasOwnPropertySuccess()
    {
        obj.result = obj.hasOwnProperty("someIntProperty");
    }

    function testHasOwnPropertyFailure()
    {
        obj.result = obj.hasOwnProperty("someNonexistentProperty");
    }

    MyTypeObject {
        id: typeObj
        objectName: "typeObj"
        pointProperty: Qt.point(34, 29)
        variantProperty: Qt.vector3d(1, 2, 3)
        stringProperty: "test string"
        property list<Rectangle> listProperty: [ Rectangle { width: 10; height: 10 } ]
        property list<Rectangle> emptyListProperty

        property bool valueTypeHasOwnProperty
        property bool valueTypeHasOwnProperty2
        property bool variantTypeHasOwnProperty
        property bool stringTypeHasOwnProperty
        property bool listTypeHasOwnProperty
        property bool listAtValidHasOwnProperty
        property bool emptyListTypeHasOwnProperty
        property bool enumTypeHasOwnProperty
        property bool typenameHasOwnProperty
        property bool typenameHasOwnProperty2
        property bool singletonTypeTypeHasOwnProperty
        property bool singletonTypePropertyTypeHasOwnProperty
        function testHasOwnPropertySuccess() {
            valueTypeHasOwnProperty = !typeObj.pointProperty.hasOwnProperty("nonexistentpropertyname");
            valueTypeHasOwnProperty2 = typeObj.pointProperty.hasOwnProperty("x"); // should be true
            variantTypeHasOwnProperty = !typeObj.variantProperty.hasOwnProperty("nonexistentpropertyname");
            stringTypeHasOwnProperty = !typeObj.stringProperty.hasOwnProperty("nonexistentpropertyname");
            listTypeHasOwnProperty = !typeObj.listProperty.hasOwnProperty("nonexistentpropertyname");
            listAtValidHasOwnProperty = !typeObj.listProperty[0].hasOwnProperty("nonexistentpropertyname");
            emptyListTypeHasOwnProperty = !typeObj.emptyListProperty.hasOwnProperty("nonexistentpropertyname");
            enumTypeHasOwnProperty = !MyTypeObject.EnumVal1.hasOwnProperty("nonexistentpropertyname");
            typenameHasOwnProperty = !MyTypeObject.hasOwnProperty("nonexistentpropertyname");
            typenameHasOwnProperty2 = MyTypeObject.hasOwnProperty("EnumVal1"); // should be true.
            singletonTypeTypeHasOwnProperty = !QtTestQObjectApi.QObject.hasOwnProperty("nonexistentpropertyname");
            singletonTypePropertyTypeHasOwnProperty = !QtTestQObjectApi.QObject.qobjectTestProperty.hasOwnProperty("nonexistentpropertyname");
        }

        property bool enumNonValueHasOwnProperty
        function testHasOwnPropertyFailureOne() {
            enumNonValueHasOwnProperty = !MyTypeObject.NonexistentEnumVal.hasOwnProperty("nonexistentpropertyname");
        }

        property bool singletonTypeNonPropertyHasOwnProperty
        function testHasOwnPropertyFailureTwo() {
            singletonTypeNonPropertyHasOwnProperty = !QtTestQObjectApi.QObject.someNonexistentProperty.hasOwnProperty("nonexistentpropertyname");
        }

        property bool listAtInvalidHasOwnProperty
        function testHasOwnPropertyFailureThree() {
            listAtInvalidHasOwnProperty = !typeObj.listProperty[5].hasOwnProperty("nonexistentpropertyname");
        }
    }
}
