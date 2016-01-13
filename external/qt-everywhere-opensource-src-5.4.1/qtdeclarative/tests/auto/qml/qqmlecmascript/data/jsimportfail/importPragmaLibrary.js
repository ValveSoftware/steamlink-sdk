.pragma library

// .pragma library, so shouldn't inherit imports from any .qml file.
function importValue() {
    var i = 3;
    var errorIsOne = Component.error == 1; // this line should fail.
    if (errorIsOne == true) {
        i = i + 4;
    }
    return i;
}
