function fact1(n) {
    if (n > 0)
        return n * fact1(n - 1);
    else
        return 1
}

function fact2(n) {
    return n > 0 ? n * fact2(n - 1) : 1
}

function fact3(n) {
    var res = 1;
    for (var i = 2; i <= n; i=i+1)
        res = res * i;
    return res;
}

print("fact1(12) =", fact1(12))
print("fact2(12) =", fact2(12))
print("fact3(12) =", fact3(12))

