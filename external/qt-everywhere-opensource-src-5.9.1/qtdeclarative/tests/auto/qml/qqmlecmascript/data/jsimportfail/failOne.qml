import QtQuick 2.0

import "testScriptImport.js" as TestScriptImport

QtObject {
    property string importScriptFunctionValue: TestScriptImport.ImportOneJs.greetingString() // should fail - the context of TestScriptImport is private to TestScriptImport.
}
