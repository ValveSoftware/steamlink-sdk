import Qt.test 1.0

MyQmlObject {
    value: {
        var value = 0
        switch (stringProperty) {
        case "A":
            value = value + 1
            value = value + 1
            /* should fall through */
        case "S":
            value = value + 1
            value = value + 1
            value = value + 1
            break;
        case "D": { // with curly braces
            value = value + 1
            value = value + 1
            value = value + 1
            break;
        }
        case "F": {
            value = value + 1
            value = value + 1
            value = value + 1
        }
        /* should fall through */
        default:
            value = value + 1
        }
    }
}

