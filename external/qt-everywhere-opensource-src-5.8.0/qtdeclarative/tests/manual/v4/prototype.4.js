
function Point(x, y) {
    this.x = x
    this.y = y
}

var pt = new Point(10, 20)
print(Point.prototype.isPrototypeOf(pt))

