import QtQuick 2.0

QtObject {
    property list<MyType> myList

    myList: [ MyType { a: 1 },
              MyType { a: 9 } ]

}
