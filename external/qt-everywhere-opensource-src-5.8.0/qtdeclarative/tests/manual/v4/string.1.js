
var s = String(123) + 1
print(s)

var s2 = new String(123)
print(s2, s2.toString, s2.toString())

var s3 = String.prototype.constructor(321)
print(s3)

var s4 = new String.prototype.constructor(321)
print(s4)

print(s4.charAt(0), s4.charAt(1), s4.charCodeAt(0))

print(s4.concat("ciao", "cool"))
print(s4.indexOf('1'))
print(s4.lastIndexOf('1', 1))

print("ss", s4.slice(0, 2))
