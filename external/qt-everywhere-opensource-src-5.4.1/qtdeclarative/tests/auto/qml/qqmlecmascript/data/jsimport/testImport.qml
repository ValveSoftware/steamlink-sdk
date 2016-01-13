import QtQuick 2.0

import "testScriptImport.js" as TestScriptImport
import "testModuleImport.js" as TestModuleImport

QtObject {
    id: testQtObject

    property string importedScriptStringValue: TestScriptImport.greetingText
    property int importedScriptFunctionValue: TestScriptImport.randomInteger(1, 20)

    property int importedModuleAttachedPropertyValue: TestModuleImport.importedAttachedPropertyValue(testQtObject)
    property int importedModuleEnumValue: TestModuleImport.importedEnumValue
}
