import QtQuick 2.0

import "testModuleImport.js" as TestModuleImport

QtObject {
    id: testQtObject
    property int importedModuleAttachedPropertyValue: testQtObject.TestModuleImport.JsQtTest.MyQmlObject.value // should fail - the context of TestScriptImport is private to TestScriptImport.
}
