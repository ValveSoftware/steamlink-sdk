
function foo() {}
foo.prototype = {}

var f = new foo()
print(foo.prototype)
print(f.__proto__)
print(foo.prototype == f.__proto__)
print(f instanceof foo)
