var y = new Float32Array(3);
y.set([1.3, 2.1, 3.5]);
x = new Uint8ClampedArray(256);
x.set([-3, 2.5, 3.5, 256]);
x.set(y, 5);
print(x.length, x.byteLength, x[0], x[1], x[2], x[3], x[5], x[6], x[7])
x = new Int16Array(x);
print(x.length, x.byteLength, x[0], x[1], x[2], x[3], x[5], x[6], x[7])
x = x.subarray(1, 2);
print(x)
print(x.length, x.byteLength, x.byteOffset, x[0], x[1])//, x[2], x[3], x[5], x[6], x[7])
x = new Int8Array([-1, 0, 3, 127, 255]);
print(x.length, x.byteLength, x.byteOffset, x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7])
