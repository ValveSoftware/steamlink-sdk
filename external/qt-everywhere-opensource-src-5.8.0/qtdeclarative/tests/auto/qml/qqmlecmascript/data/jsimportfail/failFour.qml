import QtQuick 2.0

import "testModuleImport.js" as TestModuleImport

QtObject {
    property int importedModuleEnumValue: JsQtTest.MyQmlObject.EnumValue3 // should fail - the typenames available in TestModuleImport should not be available in this scope
}
