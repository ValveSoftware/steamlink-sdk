
function foo() {
    this.stuff()
}

foo.prototype.stuff = function() {
    print("this is cool stuff")
}

f = new foo()
