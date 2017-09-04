
function test(a) {
    switch (a) {
    default: return "cool"
    case "xxx": print("got zero"); break;
    case "ciao": case "ciao2": return 123
    case "hello": return 321
    }
    return 444
}

print(test("xxx"))
print(test("hello"))
print(test("ciao"))
print(test("ciao2"))
print(test(9))
