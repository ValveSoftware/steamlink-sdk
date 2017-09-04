
function foo(a,b,c) {
    print("hello",a,b,c)
}

foo.call(null, 1,2,3);

[10,20,30].forEach(function (v,k,o) { print(v,k,o); });

print([10, 20, 30].every(function (v,k,o) { return v > 9 }));
print([10, 20, 30].some(function (v,k,o) { return v == 20 }));
print([10, 20, 30].map(function (v,k,o) { return v * v }));
print([10, 20, 30].filter(function (v,k,o) { return v >= 20 }));

print([10,20,30].reduce(function(a,v,k,o) { return a + v }));
print([10,20,30].reduceRight(function(a,v,k,o) { return a + v }));

print([10, 20, 30].find(function (v,k,o) { return v >= 20 }));
print([10, 20, 30].findIndex(function (v,k,o) { return v >= 20 }));
