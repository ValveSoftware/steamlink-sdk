.import Qt.test 1.0 as JsQtTest         // test that we can import elements from .js files

function importedAttachedPropertyValue(obj) {
    return obj.JsQtTest.MyQmlObject.value; // attached property, value = 19.
}

var importedEnumValue = JsQtTest.MyQmlObject.EnumValue3 // the actual value of this enum value is "2"

