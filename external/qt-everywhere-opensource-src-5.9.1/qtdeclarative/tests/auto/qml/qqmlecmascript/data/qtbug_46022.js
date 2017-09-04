var obj = {}
obj[5289] = 0
obj[5290] = 0
obj[5288] = 0
obj[5287] = 0
delete obj[5288]

var a = Object.getOwnPropertyNames(obj)
var test1 = a.every(function(key) {
  return obj.hasOwnProperty(key)
})

obj = {}
obj[8187] = 0
obj[8188] = 0
delete obj[8187]

var b = Object.getOwnPropertyNames(obj)
var test2 = b.every(function(key) {
  return obj.hasOwnProperty(key)
})
