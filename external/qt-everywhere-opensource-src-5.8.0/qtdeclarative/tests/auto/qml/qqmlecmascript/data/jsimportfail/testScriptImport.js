.import "importOne.js" as ImportOneJs // test that we can import scripts from .js files

var greetingText = ImportOneJs.greetingString()

function randomInteger(min, max) {
    if (max > min) {
        if (min > 10) return min;
        return max;
    }
    return min;
}
