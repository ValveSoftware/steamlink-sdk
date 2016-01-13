import QtQuick 2.0

// the type loader will implicitly search "." for a qmldir
// to try and find the missing type of AItem
// and the qmldir has various syntax errors in it.

Item {
    id: root
    AItem{}
}
