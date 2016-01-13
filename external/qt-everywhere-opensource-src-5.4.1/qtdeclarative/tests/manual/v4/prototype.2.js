
var oo = {}
function mk() {
    function zoo() {
    }
    zoo.prototype = oo
    return new zoo()
}

var a = mk()
var b = mk()
print(a.__proto__ == b.__proto__)

