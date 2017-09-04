import QtQuick 2.0

import "testScriptImport.js" as TestScriptImport

QtObject {
    property string importScriptFunctionValue: ImportOneJs.greetingString() // should fail - the typenames in TestScriptImport should not be visible from this scope
}
