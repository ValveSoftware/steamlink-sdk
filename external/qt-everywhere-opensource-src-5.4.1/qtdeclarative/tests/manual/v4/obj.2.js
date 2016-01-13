

var obj = {
    x: 10,
    y: 20,
    dump: function() { print("hello", this.x, this.y) }
}

print(obj.x, obj.y, obj.dump)
obj.dump()
