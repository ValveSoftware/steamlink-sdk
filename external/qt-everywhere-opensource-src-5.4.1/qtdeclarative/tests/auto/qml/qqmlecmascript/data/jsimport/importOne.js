.import "importTwo.js" as ImportTwoJs
.import "importThree.js" as ImportThreeJs

function greetingString() {
    if (ImportTwoJs.greetingString().length > 0) {
        return ImportTwoJs.greetingString();
    }
    return ImportThreeJs.greetingString();
}

function importOneFunction() {
    return '1';
}
