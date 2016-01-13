var x = 0
for (var i = 0; i < 20; i = i + 1) {
    if (! (i % 2))
        continue
    x = x + i
    print((i + ") x = " + x))
}
