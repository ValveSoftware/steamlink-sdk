
function foo(a) {
    x = 1
    if (a)
        throw 100;
    print("unreachable.1")
}

function bar(a) {
    print("reachable");
    foo(a)
    print("unreachable.2")
}

bar(1)
print("unreachable.3")
