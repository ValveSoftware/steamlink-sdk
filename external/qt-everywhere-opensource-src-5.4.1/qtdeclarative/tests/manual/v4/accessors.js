"use strict";
var foo = { y: 1 }
foo.__defineSetter__("x", function(x) { this.y = x;} )
foo.__defineGetter__("x", function() { return this.y;} )
print(foo.y);
foo.x = "ok";
print(foo.x);
print(foo.y);
