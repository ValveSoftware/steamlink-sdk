.pragma library
.import "importFive.js" as ImportFive

var i = 4;

function importIncrementedValue() {
    i = i + 1;
    return (i + ImportFive.importFiveFunction()); // i + '5' (not i+5)
}
