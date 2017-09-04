
var a = 1
var b = 2
var c = 10
var d = 100

var d1 = new Date
for (var i = 0; i < 1000000; i = i + 1) {
    if (a == 1)
        d = d + a + b * c
    else
        d = 321
}

print("the result is", d, "done in", new Date - d1)
