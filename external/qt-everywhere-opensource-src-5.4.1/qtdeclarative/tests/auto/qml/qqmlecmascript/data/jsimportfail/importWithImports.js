.import "importOne.js" as ImportOne

// This js file has imports, so should not inherit
// scope from the QML file which includes it.

function componentError() {
    var i = 3;
    var errorIsOne = Component.error == 1; // this line should fail.
    if (errorIsOne == true) {
        i = i + 4;
    }
    return i;
}
