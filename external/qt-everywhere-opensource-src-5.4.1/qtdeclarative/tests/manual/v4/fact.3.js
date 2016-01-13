function fact(n) {
    var res = 1;
    for (var i = 2; i <= n; i=i+1) {
        res = res * i;
    }
    return res;
}

function go() {
    var d1 = +new Date
    for (var i = 0; i < 1000000; i = i + 1) {
        if (fact(12) != 479001600)
            print(i);
    }
    var d2 = +new Date
    print("done in", d2 - d1)
}

//print(fact(12));
go();
