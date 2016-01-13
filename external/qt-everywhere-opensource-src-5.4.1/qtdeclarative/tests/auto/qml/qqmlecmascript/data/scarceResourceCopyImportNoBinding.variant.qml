import QtQuick 2.0
import Qt.test 1.0

// the following js import doesn't manually preserve or destroy any resources
import "scarceResourceCopyImportNoBinding.variant.js" as ScarceResourceCopyImportNoBindingJs

QtObject {
    // in this case, there is an import but no binding evaluated.
    // nonetheless, any resources which are not preserved, should
    // be automatically released by the engine.
}

