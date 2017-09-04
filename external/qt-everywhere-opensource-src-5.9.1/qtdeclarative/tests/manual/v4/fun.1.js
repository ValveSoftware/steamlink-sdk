
function foo(a,b) { print(a, b) }
var foo2 = foo
var foo3 = function(a,b) { print(a, b); }
foo(1,2)
foo2(1,2)
foo3(1, 2)
