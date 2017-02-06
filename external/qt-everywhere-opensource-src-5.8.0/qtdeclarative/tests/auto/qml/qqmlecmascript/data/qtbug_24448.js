var test = false;
try {
    eval(foo);
} catch (e) {
    test = (typeof foo) === "undefined";
}
