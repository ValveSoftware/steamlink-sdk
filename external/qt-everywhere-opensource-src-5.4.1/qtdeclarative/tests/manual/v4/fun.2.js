
function Point(x,y) {
    this.x = 10
    this.y = 20
}

Point.prototype = {
    print: function() { print(this.x, this.y) }
}

var pt = new Point(10, 20)
print(pt.x, pt.y)
pt.print()

