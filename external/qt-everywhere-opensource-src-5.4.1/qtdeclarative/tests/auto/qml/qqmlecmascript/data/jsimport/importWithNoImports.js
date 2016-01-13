// This js file has no imports, and so should inherit
// scope from the QML file which includes it.

function componentError() {
    var i = 5;
    var errorIsOne = Component.error == 1;
    if (errorIsOne == true) {
        i = i + 7;
    }
    return i;
}
