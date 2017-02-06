.pragma library
.import "importPragmaLibrary.js" as LibraryImport

var i = 10;

function importIncrementedValue() {
    i = i + 1;
    // because LibraryImport is shared, and used in previous tests,
    // the value will be large (already incremented a bunch of times).
    return (i + LibraryImport.importIncrementedValue());
}
