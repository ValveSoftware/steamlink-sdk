var re = new RegExp("abc")
print(re)
var match = re.exec("xxxabc")
print(match.length, match.index, match[0])
