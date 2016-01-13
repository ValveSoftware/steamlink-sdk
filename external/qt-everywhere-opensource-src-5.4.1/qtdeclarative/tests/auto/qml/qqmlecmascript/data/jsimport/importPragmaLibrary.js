.pragma library

var i = 4;

// .pragma library, so should be callable from multiple .qml with shared i.
function importIncrementedValue() {
    i = i + 1;
    return i;
}
