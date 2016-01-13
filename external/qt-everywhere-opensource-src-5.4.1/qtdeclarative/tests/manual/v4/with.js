var o = { "x": 1 }
var x = 0;
with(o) {
    with( { "x": 2 } ) {
        print(x)
    }
    print(x)
}
print(x)


function foo() {
    var x = 0;
    with(o) {
        with( { "x": 2 } ) {
            print(x)
        }
        print(x)
    }
    print(x)
}

print("\n")
foo();


function bar() {
    var x = 0;
    try {
        with(o) {
            with( { "x": 2 } ) {
                print(x)
                throw 0;
            }
            print(x)
        }
    }
    catch(e) {}
    print(x)
}

print("\n")
bar();
