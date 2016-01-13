import QtQuick 2.0

import "importPragmaLibrary.js" as TestPragmaLibraryImport

Rectangle {
    width: TestPragmaLibraryImport.importIncrementedValue()
    height: width + 15
    color: "red"
}
