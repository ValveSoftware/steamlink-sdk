import Qt.test 1.0

MyQmlObject {
    function one(kind) { return 123 }
    function two(kind) { return 321 }

    value: switch (stringProperty) {
               case "A": case "S": one(stringProperty); break;
               case "D": case "F": two(stringProperty); break;
               default: 0
           }
}

