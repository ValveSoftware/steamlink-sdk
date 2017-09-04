import QtQuick 2.0

QtObject {
    property variant n1:  0xFe32   // hex
    property variant n2:  015
    property variant n3:  -4.2E11  // floating-point literals
    property variant n4:  .1e9
    property variant n5:  3e-12
    property variant n6:  3e+12
    property variant n7:  0.1e9
    property variant n8:  1152921504606846976
    property variant n9:  100000000000000000000

    property variant c1:  "\b"     // special characters
    property variant c2:  "\f"
    property variant c3:  "\n"
    property variant c4:  "\r"
    property variant c5:  "\t"
    property variant c6:  "\v"
    property variant c7:  "\'"
    property variant c8:  "\""
    property variant c9:  "\\"
    property variant c10:  "\xA9"
    property variant c11:  "\u00A9" // unicode
}
