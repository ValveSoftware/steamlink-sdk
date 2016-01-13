import QtQuick 2.0
import "../../SpecificComponent"
import "NestedDirTwo"

Item {
    property int a
    property NDComponentTwo b: NDComponentTwo { }
    property SpecificComponent sc: SpecificComponent { }
}
