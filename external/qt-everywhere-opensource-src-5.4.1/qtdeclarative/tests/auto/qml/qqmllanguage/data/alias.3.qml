import QtQuick 2.0

QtObject {
   property variant other
   other: Alias { id: myAliasObject }

   property alias value: myAliasObject.aliasValue
   property alias value2: myAliasObject.value
}

