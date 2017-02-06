import QtQuick 2.0
import Qt.test 1.0

Item {

    MyStringClass {
        id: msc
    }

    function compare(a0, a1) {
        var compareOk = a0.length === a1.length;

        if (compareOk === true) {
            for (var i=0 ; i < a0.length ; ++i) {
                if (a0[i] != a1[i]) {
                    compareOk = false;
                    break;
                }
            }
        }

        return compareOk;
    }

    function compareStrings(a, b) {
        return (a == b) ? 0 : ((a < b) ? 1 : -1);
    }

    function compareNumbers(a, b) {
        return a - b;
    }

    function createExpected(list, sortFn) {
        var expected = []
        for (var i=0 ; i < list.length ; ++i)
            expected.push(list[i]);
        if (sortFn === null)
            expected.sort();
        else
            expected.sort(sortFn);
        return expected;
    }

    function checkResults(expected, actual, sortFn) {
        if (sortFn === null)
            actual.sort();
        else
            actual.sort(sortFn);
        return compare(expected, actual);
    }

    function doStringTest(stringList, fn) {
        var expected = createExpected(stringList, fn);
        var actual = msc.strings(stringList);
        return checkResults(expected, actual, fn);
    }
    function doIntTest(intList, fn) {
        var expected = createExpected(intList, fn);
        var actual = msc.integers(intList);
        return checkResults(expected, actual, fn);
    }
    function doRealTest(realList, fn) {
        var expected = createExpected(realList, fn);
        var actual = msc.reals(realList);
        return checkResults(expected, actual, fn);
    }
    function doIntVectorTest(intList, fn) {
        var expected = createExpected(intList, fn);
        var actual = msc.integerVector(intList);
        return checkResults(expected, actual, fn);
    }
    function doRealVectorTest(realList, fn) {
        var expected = createExpected(realList, fn);
        var actual = msc.realVector(realList);
        return checkResults(expected, actual, fn);
    }

    function test_qtbug_25269(useCustomCompare) {
        return doStringTest( [ "one", "two", "three" ], null );
    }
    function test_alphabet_insertionSort(useCustomCompare) {
        var fn = useCustomCompare ? compareStrings : null;
        return doStringTest( [ "z", "y", "M", "a", "c", "B" ], fn );
    }
    function test_alphabet_quickSort(useCustomCompare) {
        var fn = useCustomCompare ? compareStrings : null;
        return doStringTest( [ "z", "y", "m", "a", "c", "B", "gG", "u", "bh", "lk", "GW", "Z", "n", "nm", "oi", "njk", "f", "dd", "ooo", "3des", "num123", "ojh", "lkj", "a6^^", "bl!!", "!o" ], fn );
    }
    function test_numbers_insertionSort(useCustomCompare) {
        var fn = useCustomCompare ? compareNumbers : null;
        return doIntTest( [ 7, 3, 9, 1, 0, -1, 20, -11 ], fn );
    }
    function test_numbers_quickSort(useCustomCompare) {
        var fn = useCustomCompare ? compareNumbers : null;
        return doIntTest( [ 7, 3, 37, 9, 1, 0, -1, 20, -11, -300, -87, 1, 3, -2, 100, 108, 96, 9, 99999, 12, 11, 11, 12, 11, 13, -13, 10, 10, 10, 8, 12 ], fn );
    }
    function test_reals_insertionSort(useCustomCompare) {
        var fn = useCustomCompare ? compareNumbers : null;
        return doRealTest( [ -3.4, 1, 10, 4.23, -30.1, 4.24, 4.21, -1, -1 ], fn );
    }
    function test_reals_quickSort(useCustomCompare) {
        var fn = useCustomCompare ? compareNumbers : null;
        return doRealTest( [ -3.4, 1, 10, 4.23, -30.1, 4.24, 4.21, -1, -1, 12, -100, 87.4, 101.3, -8.88888, 7.76, 10.10, 1.1, -1.1, -0, 11, 12.8, 0.001, -11, -0.75, 99999.99, 11.12, 32.3, 3.333333, 9.876 ], fn );
    }
    function test_number_vector_insertionSort(useCustomCompare) {
        var fn = useCustomCompare ? compareNumbers : null;
        return doIntVectorTest( [ 7, 3, 9, 1, 0, -1, 20, -11 ], fn );
    }
    function test_number_vector_quickSort(useCustomCompare) {
        var fn = useCustomCompare ? compareNumbers : null;
        return doIntVectorTest( [ 7, 3, 37, 9, 1, 0, -1, 20, -11, -300, -87, 1, 3, -2, 100, 108, 96, 9, 99999, 12, 11, 11, 12, 11, 13, -13, 10, 10, 10, 8, 12 ], fn );
    }
    function test_real_vector_insertionSort(useCustomCompare) {
        var fn = useCustomCompare ? compareNumbers : null;
        return doRealVectorTest( [ -3.4, 1, 10, 4.23, -30.1, 4.24, 4.21, -1, -1 ], fn );
    }
    function test_real_vector_quickSort(useCustomCompare) {
        var fn = useCustomCompare ? compareNumbers : null;
        return doRealVectorTest( [ -3.4, 1, 10, 4.23, -30.1, 4.24, 4.21, -1, -1, 12, -100, 87.4, 101.3, -8.88888, 7.76, 10.10, 1.1, -1.1, -0, 11, 12.8, 0.001, -11, -0.75, 99999.99, 11.12, 32.3, 3.333333, 9.876 ], fn );
    }
}
