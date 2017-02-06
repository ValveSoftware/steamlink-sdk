
function fix(f) {
    var k = function (x) {
        return f(function (z) { return x(x)(z) })
    }
    return k(k)
}

var F = function (f) {
    return function (n) {
        return n == 0 ? 1 : n * f(n - 1)
    }
}

var fact = fix(F)

print("the factorial of 12 is", fact(12))
