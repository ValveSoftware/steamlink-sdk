var i;
for (i = 0; i < 2; ++i) {
    switch ("a") {
        case "a":
            continue;
        default:
            break;
    }
}
if (i != 2)
    print("loop did not run often enough");
