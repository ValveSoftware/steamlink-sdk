function foo() {
    var obj = { x: 10 }

    for (var i = 0; i < 1000000; ++i) {
        var y = obj.x;
        obj.x = y;
    }
}
foo();
