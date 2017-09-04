
var d1 = new Date()
a = []

for (var i = 0; i < 10000; i = i + 1)
    a[i] = i

for (var i = 0; i < 10000; i = i + 1)
    if (a[i] != i)
        print("KO", i)

print("done in", new Date - d1)

var s = "some cool stuff"
for (var i = 0; i < 15; i = i + 1) {
    print(s[i])
}

var xx = [1,2]
xx[0] = 1
print(xx.length)
print(xx)

print("yy", xx.concat(xx))
var pp = [1,2,3,4]
var aa = pp.concat([5,6,7]);
print(aa[0], aa[1], aa[2], aa[3], aa[4], aa[5], aa.length)
print(Array.prototype.concat("hello", true, 123, null, "world"))
aa.pop()
print(aa)
aa.pop()
print(aa)
