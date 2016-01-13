import QtQuick 2.0
import Test 1.0

Item {
    MyEnum1Class {
        id: enum1Class
        objectName: "enum1Class"
    }

    MyEnumDerivedClass {
        id: enumDerivedClass
        objectName: "enumDerivedClass"

        onValueAChanged: {
            aValue = newValue;
        }

        onValueBChanged: {
            bValue = newValue;
        }

        onValueCChanged: {
            cValue = newValue;
        }

        onValueDChanged: {
            dValue = newValue;
        }

        onValueEChanged: {
            eValue = newValue;
        }

        onValueE2Changed: {
            e2Value = newValue;
        }

        property int aValue: 0
        property int bValue: 0
        property int cValue: 0
        property int dValue: 0
        property int eValue: 0
        property int e2Value: 0
    }

    function setEnumValues() {
        enum1Class.setValue(MyEnum1Class.A_13);
        enumDerivedClass.setValueA(MyEnum1Class.A_11);
        enumDerivedClass.setValueB(MyEnum2Class.B_37);
        enumDerivedClass.setValueC(Qt.RichText);
        enumDerivedClass.setValueD(Qt.ElideMiddle);
        enumDerivedClass.setValueE(MyEnum2Class.E_14);
        enumDerivedClass.setValueE2(MyEnum2Class.E_76);
    }
}
