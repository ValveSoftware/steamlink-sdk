function sum(ba) {
    var result = 0;
    for (var i = 0; i < ba.length; ++i)
        result += ba[i];
    return result;
}

sum(new ByteArray(10000));
