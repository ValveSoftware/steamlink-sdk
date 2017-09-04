import QtQuick 2.0

import com.nokia.JsModule 1.0
import com.nokia.JsModule 1.0 as RenamedModule
import "testJsModuleRemoteImport.js" as TestJsModuleImport

QtObject {
    id: testQtObject

    property string importedScriptStringValue: ScriptAPI.greeting();
    property string renamedScriptStringValue: RenamedModule.ScriptAPI.greeting();
    property string reimportedScriptStringValue: TestJsModuleImport.importedValue();
}
