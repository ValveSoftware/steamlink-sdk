
function fact(n) {
    return n > 1 ? n * fact(n - 1) : 1
}

for (var i = 0; i < 1000000; i = i + 1)
    fact(12)

