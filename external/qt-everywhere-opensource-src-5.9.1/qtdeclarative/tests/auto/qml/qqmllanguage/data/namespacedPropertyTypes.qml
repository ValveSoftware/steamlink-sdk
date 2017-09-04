import QtQuick 2.0 as MyQuick

MyQuick.Item {
    property MyQuick.Item myProp;
    property list<MyQuick.Item> myList;
    default property list<MyQuick.Item> myDefaultList;
    signal mySignal(MyQuick.Item someItem)
}
