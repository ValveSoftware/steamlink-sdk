import QtQuick 2.0
import LaterImports 1.0

TestElement {
    id: deleteme
    function testFn() {
        gc(); // at this point, obj is deleted.
        dangerousFunction(); // calling this function will throw an exeption
        // because this object has been deleted and its context is not available

        // which means that we shouldn't get to this line.
        row.test10_1 = 1;
    }
}
