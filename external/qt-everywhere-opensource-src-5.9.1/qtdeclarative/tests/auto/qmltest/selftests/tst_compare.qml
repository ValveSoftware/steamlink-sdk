/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtTest 1.1

TestCase {
    name: "SelfTests_compare"

    function test_primitives_and_constants() {
        compare(qtest_compareInternal(null, null), true, "null");
        compare(qtest_compareInternal(null, {}), false, "null");
        compare(qtest_compareInternal(null, undefined), false, "null");
        compare(qtest_compareInternal(null, 0), false, "null");
        compare(qtest_compareInternal(null, false), false, "null");
        compare(qtest_compareInternal(null, ''), false, "null");
        compare(qtest_compareInternal(null, []), false, "null");

        compare(qtest_compareInternal(undefined, undefined), true, "undefined");
        compare(qtest_compareInternal(undefined, null), false, "undefined");
        compare(qtest_compareInternal(undefined, 0), false, "undefined");
        compare(qtest_compareInternal(undefined, false), false, "undefined");
        compare(qtest_compareInternal(undefined, {}), false, "undefined");
        compare(qtest_compareInternal(undefined, []), false, "undefined");
        compare(qtest_compareInternal(undefined, ""), false, "undefined");

        // Nan usually doest not equal to Nan using the '==' operator.
        // Only isNaN() is able to do it.
        compare(qtest_compareInternal(0/0, 0/0), true, "NaN"); // NaN VS NaN
        compare(qtest_compareInternal(1/0, 2/0), true, "Infinity"); // Infinity VS Infinity
        compare(qtest_compareInternal(-1/0, 2/0), false, "-Infinity, Infinity"); // -Infinity VS Infinity
        compare(qtest_compareInternal(-1/0, -2/0), true, "-Infinity, -Infinity"); // -Infinity VS -Infinity
        compare(qtest_compareInternal(0/0, 1/0), false, "NaN, Infinity"); // Nan VS Infinity
        compare(qtest_compareInternal(1/0, 0/0), false, "NaN, Infinity"); // Nan VS Infinity
        compare(qtest_compareInternal(0/0, null), false, "NaN");
        compare(qtest_compareInternal(0/0, undefined), false, "NaN");
        compare(qtest_compareInternal(0/0, 0), false, "NaN");
        compare(qtest_compareInternal(0/0, false), false, "NaN");
        //compare(qtest_compareInternal(0/0, function () {}), false, "NaN");       // Do we really need that?
        compare(qtest_compareInternal(1/0, null), false, "NaN, Infinity");
        compare(qtest_compareInternal(1/0, undefined), false, "NaN, Infinity");
        compare(qtest_compareInternal(1/0, 0), false, "NaN, Infinity");
        compare(qtest_compareInternal(1/0, 1), false, "NaN, Infinity");
        compare(qtest_compareInternal(1/0, false), false, "NaN, Infinity");
        compare(qtest_compareInternal(1/0, true), false, "NaN, Infinity");
        //compare(qtest_compareInternal(1/0, function () {}), false, "NaN");       // Do we really need that?

        compare(qtest_compareInternal(0, 0), true, "number");
        compare(qtest_compareInternal(0, 1), false, "number");
        compare(qtest_compareInternal(1, 0), false, "number");
        compare(qtest_compareInternal(1, 1), true, "number");
        compare(qtest_compareInternal(1.1, 1.1), true, "number");
        compare(qtest_compareInternal(0.0000005, 0.0000005), true, "number");
        compare(qtest_compareInternal(0, ''), false, "number");
        compare(qtest_compareInternal(0, '0'), false, "number");
        compare(qtest_compareInternal(1, '1'), false, "number");
        compare(qtest_compareInternal(0, false), false, "number");
        compare(qtest_compareInternal(1, true), false, "number");

        compare(qtest_compareInternal(true, true), true, "boolean");
        compare(qtest_compareInternal(true, false), false, "boolean");
        compare(qtest_compareInternal(false, true), false, "boolean");
        compare(qtest_compareInternal(false, 0), false, "boolean");
        compare(qtest_compareInternal(false, null), false, "boolean");
        compare(qtest_compareInternal(false, undefined), false, "boolean");
        compare(qtest_compareInternal(true, 1), false, "boolean");
        compare(qtest_compareInternal(true, null), false, "boolean");
        compare(qtest_compareInternal(true, undefined), false, "boolean");

        compare(qtest_compareInternal('', ''), true, "string");
        compare(qtest_compareInternal('a', 'a'), true, "string");
        compare(qtest_compareInternal("foobar", "foobar"), true, "string");
        compare(qtest_compareInternal("foobar", "foo"), false, "string");
        compare(qtest_compareInternal('', 0), false, "string");
        compare(qtest_compareInternal('', false), false, "string");
        compare(qtest_compareInternal('', null), false, "string");
        compare(qtest_compareInternal('', undefined), false, "string");

        // Short annotation VS new annotation
        compare(qtest_compareInternal(0, new Number()), true, "short annotation VS new annotation");
        compare(qtest_compareInternal(new Number(), 0), true, "short annotation VS new annotation");
        compare(qtest_compareInternal(1, new Number(1)), true, "short annotation VS new annotation");
        compare(qtest_compareInternal(new Number(1), 1), true, "short annotation VS new annotation");
        compare(qtest_compareInternal(new Number(0), 1), false, "short annotation VS new annotation");
        compare(qtest_compareInternal(0, new Number(1)), false, "short annotation VS new annotation");

        compare(qtest_compareInternal(new String(), ""), true, "short annotation VS new annotation");
        compare(qtest_compareInternal("", new String()), true, "short annotation VS new annotation");
        compare(qtest_compareInternal(new String("My String"), "My String"), true, "short annotation VS new annotation");
        compare(qtest_compareInternal("My String", new String("My String")), true, "short annotation VS new annotation");
        compare(qtest_compareInternal("Bad String", new String("My String")), false, "short annotation VS new annotation");
        compare(qtest_compareInternal(new String("Bad String"), "My String"), false, "short annotation VS new annotation");

        compare(qtest_compareInternal(false, new Boolean()), true, "short annotation VS new annotation");
        compare(qtest_compareInternal(new Boolean(), false), true, "short annotation VS new annotation");
        compare(qtest_compareInternal(true, new Boolean(true)), true, "short annotation VS new annotation");
        compare(qtest_compareInternal(new Boolean(true), true), true, "short annotation VS new annotation");
        compare(qtest_compareInternal(true, new Boolean(1)), true, "short annotation VS new annotation");
        compare(qtest_compareInternal(false, new Boolean(false)), true, "short annotation VS new annotation");
        compare(qtest_compareInternal(new Boolean(false), false), true, "short annotation VS new annotation");
        compare(qtest_compareInternal(false, new Boolean(0)), true, "short annotation VS new annotation");
        compare(qtest_compareInternal(true, new Boolean(false)), false, "short annotation VS new annotation");
        compare(qtest_compareInternal(new Boolean(false), true), false, "short annotation VS new annotation");

        compare(qtest_compareInternal(new Object(), {}), true, "short annotation VS new annotation");
        compare(qtest_compareInternal({}, new Object()), true, "short annotation VS new annotation");
        compare(qtest_compareInternal(new Object(), {a:1}), false, "short annotation VS new annotation");
        compare(qtest_compareInternal({a:1}, new Object()), false, "short annotation VS new annotation");
        compare(qtest_compareInternal({a:undefined}, new Object()), false, "short annotation VS new annotation");
        compare(qtest_compareInternal(new Object(), {a:undefined}), false, "short annotation VS new annotation");
    }

    function test_objects_basics() {
        compare(qtest_compareInternal({}, {}), true);
        compare(qtest_compareInternal({}, null), false);
        compare(qtest_compareInternal({}, undefined), false);
        compare(qtest_compareInternal({}, 0), false);
        compare(qtest_compareInternal({}, false), false);

        // This test is a hard one, it is very important
        // REASONS:
        //      1) They are of the same type "object"
        //      2) [] instanceof Object is true
        //      3) Their properties are the same (doesn't exists)
        compare(qtest_compareInternal({}, []), false);

        compare(qtest_compareInternal({a:1}, {a:1}), true);
        compare(qtest_compareInternal({a:1}, {a:"1"}), false);
        compare(qtest_compareInternal({a:[]}, {a:[]}), true);
        compare(qtest_compareInternal({a:{}}, {a:null}), false);
        compare(qtest_compareInternal({a:1}, {}), false);
        compare(qtest_compareInternal({}, {a:1}), false);

        // Hard ones
        compare(qtest_compareInternal({a:undefined}, {}), false);
        compare(qtest_compareInternal({}, {a:undefined}), false);
        compare(qtest_compareInternal(
            {
                a: [{ bar: undefined }]
            },
            {
                a: [{ bat: undefined }]
            }
        ), false);
    }

    function test_arrays_basics() {
        compare(qtest_compareInternal([], []), true);

        // May be a hard one, can invoke a crash at execution.
        // because their types are both "object" but null isn't
        // like a true object, it doesn't have any property at all.
        compare(qtest_compareInternal([], null), false);

        compare(qtest_compareInternal([], undefined), false);
        compare(qtest_compareInternal([], false), false);
        compare(qtest_compareInternal([], 0), false);
        compare(qtest_compareInternal([], ""), false);

        // May be a hard one, but less hard
        // than {} with [] (note the order)
        compare(qtest_compareInternal([], {}), false);

        compare(qtest_compareInternal([null],[]), false);
        compare(qtest_compareInternal([undefined],[]), false);
        compare(qtest_compareInternal([],[null]), false);
        compare(qtest_compareInternal([],[undefined]), false);
        compare(qtest_compareInternal([null],[undefined]), false);
        compare(qtest_compareInternal([[]],[[]]), true);
        compare(qtest_compareInternal([[],[],[]],[[],[],[]]), true);
        compare(qtest_compareInternal(
                                [[],[],[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]],
                                [[],[],[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]),
                                true);
        compare(qtest_compareInternal(
                                [[],[],[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]],
                                [[],[],[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]), // shorter
                                false);
        compare(qtest_compareInternal(
                                [[],[],[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[{}]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]],
                                [[],[],[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]), // deepest element not an array
                                false);

        // same multidimensional
        compare(qtest_compareInternal(
                                [1,2,3,4,5,6,7,8,9, [
                                    1,2,3,4,5,6,7,8,9, [
                                        1,2,3,4,5,[
                                            [6,7,8,9, [
                                                [
                                                    1,2,3,4,[
                                                        2,3,4,[
                                                            1,2,[
                                                                1,2,3,4,[
                                                                    1,2,3,4,5,6,7,8,9,[
                                                                        0
                                                                    ],1,2,3,4,5,6,7,8,9
                                                                ],5,6,7,8,9
                                                            ],4,5,6,7,8,9
                                                        ],5,6,7,8,9
                                                    ],5,6,7
                                                ]
                                            ]
                                        ]
                                    ]
                                ]]],
                                [1,2,3,4,5,6,7,8,9, [
                                    1,2,3,4,5,6,7,8,9, [
                                        1,2,3,4,5,[
                                            [6,7,8,9, [
                                                [
                                                    1,2,3,4,[
                                                        2,3,4,[
                                                            1,2,[
                                                                1,2,3,4,[
                                                                    1,2,3,4,5,6,7,8,9,[
                                                                        0
                                                                    ],1,2,3,4,5,6,7,8,9
                                                                ],5,6,7,8,9
                                                            ],4,5,6,7,8,9
                                                        ],5,6,7,8,9
                                                    ],5,6,7
                                                ]
                                            ]
                                        ]
                                    ]
                                ]]]),
                                true, "Multidimensional");

        // different multidimensional
        compare(qtest_compareInternal(
                                [1,2,3,4,5,6,7,8,9, [
                                    1,2,3,4,5,6,7,8,9, [
                                        1,2,3,4,5,[
                                            [6,7,8,9, [
                                                [
                                                    1,2,3,4,[
                                                        2,3,4,[
                                                            1,2,[
                                                                1,2,3,4,[
                                                                    1,2,3,4,5,6,7,8,9,[
                                                                        0
                                                                    ],1,2,3,4,5,6,7,8,9
                                                                ],5,6,7,8,9
                                                            ],4,5,6,7,8,9
                                                        ],5,6,7,8,9
                                                    ],5,6,7
                                                ]
                                            ]
                                        ]
                                    ]
                                ]]],
                                [1,2,3,4,5,6,7,8,9, [
                                    1,2,3,4,5,6,7,8,9, [
                                        1,2,3,4,5,[
                                            [6,7,8,9, [
                                                [
                                                    1,2,3,4,[
                                                        2,3,4,[
                                                            1,2,[
                                                                '1',2,3,4,[                 // string instead of number
                                                                    1,2,3,4,5,6,7,8,9,[
                                                                        0
                                                                    ],1,2,3,4,5,6,7,8,9
                                                                ],5,6,7,8,9
                                                            ],4,5,6,7,8,9
                                                        ],5,6,7,8,9
                                                    ],5,6,7
                                                ]
                                            ]
                                        ]
                                    ]
                                ]]]),
                                false, "Multidimensional");

        // different multidimensional
        compare(qtest_compareInternal(
                                [1,2,3,4,5,6,7,8,9, [
                                    1,2,3,4,5,6,7,8,9, [
                                        1,2,3,4,5,[
                                            [6,7,8,9, [
                                                [
                                                    1,2,3,4,[
                                                        2,3,4,[
                                                            1,2,[
                                                                1,2,3,4,[
                                                                    1,2,3,4,5,6,7,8,9,[
                                                                        0
                                                                    ],1,2,3,4,5,6,7,8,9
                                                                ],5,6,7,8,9
                                                            ],4,5,6,7,8,9
                                                        ],5,6,7,8,9
                                                    ],5,6,7
                                                ]
                                            ]
                                        ]
                                    ]
                                ]]],
                                [1,2,3,4,5,6,7,8,9, [
                                    1,2,3,4,5,6,7,8,9, [
                                        1,2,3,4,5,[
                                            [6,7,8,9, [
                                                [
                                                    1,2,3,4,[
                                                        2,3,[                   // missing an element (4)
                                                            1,2,[
                                                                1,2,3,4,[
                                                                    1,2,3,4,5,6,7,8,9,[
                                                                        0
                                                                    ],1,2,3,4,5,6,7,8,9
                                                                ],5,6,7,8,9
                                                            ],4,5,6,7,8,9
                                                        ],5,6,7,8,9
                                                    ],5,6,7
                                                ]
                                            ]
                                        ]
                                    ]
                                ]]]),
                                false, "Multidimensional");
    }

    function test_date_instances() {
        // Date, we don't need to test Date.parse() because it returns a number.
        // Only test the Date instances by setting them a fix date.
        // The date use is midnight January 1, 1970

        var d1 = new Date();
        d1.setTime(0); // fix the date

        var d2 = new Date();
        d2.setTime(0); // fix the date

        var d3 = new Date(); // The very now

        // Anyway their types differs, just in case the code fails in the order in which it deals with date
        compare(qtest_compareInternal(d1, 0), false); // d1.valueOf() returns 0, but d1 and 0 are different
        // test same values date and different instances equality
        compare(qtest_compareInternal(d1, d2), true);
        // test different date and different instances difference
        compare(qtest_compareInternal(d1, d3), false);

        // try to fool equiv by adding a valueOf function to an object
        // that would return the same value of a real date instance.
        var d4 = new Date();
        d4.setFullYear(2010);
        d4.setMonth(1);
        d4.setDate(1);
        d4.setHours(1);
        d4.setMinutes(1);
        d4.setSeconds(1);
        d4.setMilliseconds(1);
        var o4 = {
            valueOf: function () {
                return d4.valueOf();
            }
        }
        compare(qtest_compareInternal(d4, o4), false); // o4 isn't an instance of Date
    }


    function test_regexp() {
        // Must test cases that imply those traps:
        // var a = /./;
        // a instanceof Object;        // Oops
        // a instanceof RegExp;        // Oops
        // typeof a === "function";    // Oops, false in IE and Opera, true in FF and Safari ("object")

        // Tests same regex with same modifiers in different order
        var r = /foo/;
        var r5 = /foo/gim;
        var r6 = /foo/gmi;
        var r7 = /foo/igm;
        var r8 = /foo/img;
        var r9 = /foo/mig;
        var r10 = /foo/mgi;
        var ri1 = /foo/i;
        var ri2 = /foo/i;
        var rm1 = /foo/m;
        var rm2 = /foo/m;
        var rg1 = /foo/g;
        var rg2 = /foo/g;

        compare(qtest_compareInternal(r5, r6), true, "Modifier order");
        compare(qtest_compareInternal(r5, r7), true, "Modifier order");
        compare(qtest_compareInternal(r5, r8), true, "Modifier order");
        compare(qtest_compareInternal(r5, r9), true, "Modifier order");
        compare(qtest_compareInternal(r5, r10), true, "Modifier order");
        compare(qtest_compareInternal(r, r5), false, "Modifier");

        compare(qtest_compareInternal(ri1, ri2), true, "Modifier");
        compare(qtest_compareInternal(r, ri1), false, "Modifier");
        compare(qtest_compareInternal(ri1, rm1), false, "Modifier");
        compare(qtest_compareInternal(r, rm1), false, "Modifier");
        compare(qtest_compareInternal(rm1, ri1), false, "Modifier");
        compare(qtest_compareInternal(rm1, rm2), true, "Modifier");
        compare(qtest_compareInternal(rg1, rm1), false, "Modifier");
        compare(qtest_compareInternal(rm1, rg1), false, "Modifier");
        compare(qtest_compareInternal(rg1, rg2), true, "Modifier");

        // Different regex, same modifiers
        var r11 = /[a-z]/gi;
        var r13 = /[0-9]/gi; // oops! different
        compare(qtest_compareInternal(r11, r13), false, "Regex pattern");

        var r14 = /0/ig;
        var r15 = /"0"/ig; // oops! different
        compare(qtest_compareInternal(r14, r15), false, "Regex pattern");

        var r1 = /[\n\r\u2028\u2029]/g;
        var r2 = /[\n\r\u2028\u2029]/g;
        var r3 = /[\n\r\u2028\u2028]/g; // differs from r1
        var r4 = /[\n\r\u2028\u2029]/;  // differs from r1

        compare(qtest_compareInternal(r1, r2), true, "Regex pattern");
        compare(qtest_compareInternal(r1, r3), false, "Regex pattern");
        compare(qtest_compareInternal(r1, r4), false, "Regex pattern");

        // More complex regex
        var regex1 = "^[-_.a-z0-9]+@([-_a-z0-9]+\\.)+([A-Za-z][A-Za-z]|[A-Za-z][A-Za-z][A-Za-z])|(([0-9][0-9]?|[0-1][0-9][0-9]|[2][0-4][0-9]|[2][5][0-5]))$";
        var regex2 = "^[-_.a-z0-9]+@([-_a-z0-9]+\\.)+([A-Za-z][A-Za-z]|[A-Za-z][A-Za-z][A-Za-z])|(([0-9][0-9]?|[0-1][0-9][0-9]|[2][0-4][0-9]|[2][5][0-5]))$";
        // regex 3 is different: '.' not escaped
        var regex3 = "^[-_.a-z0-9]+@([-_a-z0-9]+.)+([A-Za-z][A-Za-z]|[A-Za-z][A-Za-z][A-Za-z])|(([0-9][0-9]?|[0-1][0-9][0-9]|[2][0-4][0-9]|[2][5][0-5]))$";

        var r21 = new RegExp(regex1);
        var r22 = new RegExp(regex2);
        var r23 = new RegExp(regex3); // diff from r21, not same pattern
        var r23a = new RegExp(regex3, "gi"); // diff from r23, not same modifier
        var r24a = new RegExp(regex3, "ig"); // same as r23a

        compare(qtest_compareInternal(r21, r22), true, "Complex Regex");
        compare(qtest_compareInternal(r21, r23), false, "Complex Regex");
        compare(qtest_compareInternal(r23, r23a), false, "Complex Regex");
        compare(qtest_compareInternal(r23a, r24a), true, "Complex Regex");

        // typeof r1 is "function" in some browsers and "object" in others so we must cover this test
        var re = / /;
        compare(qtest_compareInternal(re, function () {}), false, "Regex internal");
        compare(qtest_compareInternal(re, {}), false, "Regex internal");
    }


    function test_complex_objects() {

        function fn1() {
            return "fn1";
        }
        function fn2() {
            return "fn2";
        }

        // Try to invert the order of some properties to make sure it is covered.
        // It can failed when properties are compared between unsorted arrays.
        compare(qtest_compareInternal(
            {
                a: 1,
                b: null,
                c: [{}],
                d: {
                    a: 3.14159,
                    b: false,
                    c: {
                        e: fn1,
                        f: [[[]]],
                        g: {
                            j: {
                                k: {
                                    n: {
                                        r: "r",
                                        s: [1,2,3],
                                        t: undefined,
                                        u: 0,
                                        v: {
                                            w: {
                                                x: {
                                                    y: "Yahoo!",
                                                    z: null
                                                }
                                            }
                                        }
                                    },
                                    q: [],
                                    p: 1/0,
                                    o: 99
                                },
                                l: undefined,
                                m: null
                            }
                        },
                        d: 0,
                        i: true,
                        h: "false"
                    }
                },
                e: undefined,
                g: "",
                h: "h",
                f: {},
                i: []
            },
            {
                a: 1,
                b: null,
                c: [{}],
                d: {
                    b: false,
                    a: 3.14159,
                    c: {
                        d: 0,
                        e: fn1,
                        f: [[[]]],
                        g: {
                            j: {
                                k: {
                                    n: {
                                        r: "r",
                                        t: undefined,
                                        u: 0,
                                        s: [1,2,3],
                                        v: {
                                            w: {
                                                x: {
                                                    z: null,
                                                    y: "Yahoo!"
                                                }
                                            }
                                        }
                                    },
                                    o: 99,
                                    p: 1/0,
                                    q: []
                                },
                                l: undefined,
                                m: null
                            }
                        },
                        i: true,
                        h: "false"
                    }
                },
                e: undefined,
                g: "",
                f: {},
                h: "h",
                i: []
            }
        ), true);

        compare(qtest_compareInternal(
            {
                a: 1,
                b: null,
                c: [{}],
                d: {
                    a: 3.14159,
                    b: false,
                    c: {
                        d: 0,
                        e: fn1,
                        f: [[[]]],
                        g: {
                            j: {
                                k: {
                                    n: {
                                        //r: "r",   // different: missing a property
                                        s: [1,2,3],
                                        t: undefined,
                                        u: 0,
                                        v: {
                                            w: {
                                                x: {
                                                    y: "Yahoo!",
                                                    z: null
                                                }
                                            }
                                        }
                                    },
                                    o: 99,
                                    p: 1/0,
                                    q: []
                                },
                                l: undefined,
                                m: null
                            }
                        },
                        h: "false",
                        i: true
                    }
                },
                e: undefined,
                f: {},
                g: "",
                h: "h",
                i: []
            },
            {
                a: 1,
                b: null,
                c: [{}],
                d: {
                    a: 3.14159,
                    b: false,
                    c: {
                        d: 0,
                        e: fn1,
                        f: [[[]]],
                        g: {
                            j: {
                                k: {
                                    n: {
                                        r: "r",
                                        s: [1,2,3],
                                        t: undefined,
                                        u: 0,
                                        v: {
                                            w: {
                                                x: {
                                                    y: "Yahoo!",
                                                    z: null
                                                }
                                            }
                                        }
                                    },
                                    o: 99,
                                    p: 1/0,
                                    q: []
                                },
                                l: undefined,
                                m: null
                            }
                        },
                        h: "false",
                        i: true
                    }
                },
                e: undefined,
                f: {},
                g: "",
                h: "h",
                i: []
            }
        ), false);

        compare(qtest_compareInternal(
            {
                a: 1,
                b: null,
                c: [{}],
                d: {
                    a: 3.14159,
                    b: false,
                    c: {
                        d: 0,
                        e: fn1,
                        f: [[[]]],
                        g: {
                            j: {
                                k: {
                                    n: {
                                        r: "r",
                                        s: [1,2,3],
                                        t: undefined,
                                        u: 0,
                                        v: {
                                            w: {
                                                x: {
                                                    y: "Yahoo!",
                                                    z: null
                                                }
                                            }
                                        }
                                    },
                                    o: 99,
                                    p: 1/0,
                                    q: []
                                },
                                l: undefined,
                                m: null
                            }
                        },
                        h: "false",
                        i: true
                    }
                },
                e: undefined,
                f: {},
                g: "",
                h: "h",
                i: []
            },
            {
                a: 1,
                b: null,
                c: [{}],
                d: {
                    a: 3.14159,
                    b: false,
                    c: {
                        d: 0,
                        e: fn1,
                        f: [[[]]],
                        g: {
                            j: {
                                k: {
                                    n: {
                                        r: "r",
                                        s: [1,2,3],
                                        //t: undefined,                 // different: missing a property with an undefined value
                                        u: 0,
                                        v: {
                                            w: {
                                                x: {
                                                    y: "Yahoo!",
                                                    z: null
                                                }
                                            }
                                        }
                                    },
                                    o: 99,
                                    p: 1/0,
                                    q: []
                                },
                                l: undefined,
                                m: null
                            }
                        },
                        h: "false",
                        i: true
                    }
                },
                e: undefined,
                f: {},
                g: "",
                h: "h",
                i: []
            }
        ), false);

        compare(qtest_compareInternal(
            {
                a: 1,
                b: null,
                c: [{}],
                d: {
                    a: 3.14159,
                    b: false,
                    c: {
                        d: 0,
                        e: fn1,
                        f: [[[]]],
                        g: {
                            j: {
                                k: {
                                    n: {
                                        r: "r",
                                        s: [1,2,3],
                                        t: undefined,
                                        u: 0,
                                        v: {
                                            w: {
                                                x: {
                                                    y: "Yahoo!",
                                                    z: null
                                                }
                                            }
                                        }
                                    },
                                    o: 99,
                                    p: 1/0,
                                    q: []
                                },
                                l: undefined,
                                m: null
                            }
                        },
                        h: "false",
                        i: true
                    }
                },
                e: undefined,
                f: {},
                g: "",
                h: "h",
                i: []
            },
            {
                a: 1,
                b: null,
                c: [{}],
                d: {
                    a: 3.14159,
                    b: false,
                    c: {
                        d: 0,
                        e: fn1,
                        f: [[[]]],
                        g: {
                            j: {
                                k: {
                                    n: {
                                        r: "r",
                                        s: [1,2,3],
                                        t: undefined,
                                        u: 0,
                                        v: {
                                            w: {
                                                x: {
                                                    y: "Yahoo!",
                                                    z: null
                                                }
                                            }
                                        }
                                    },
                                    o: 99,
                                    p: 1/0,
                                    q: {}           // different was []
                                },
                                l: undefined,
                                m: null
                            }
                        },
                        h: "false",
                        i: true
                    }
                },
                e: undefined,
                f: {},
                g: "",
                h: "h",
                i: []
            }
        ), false);

        var same1 = {
            a: [
                "string", null, 0, "1", 1, {
                    prop: null,
                    foo: [1,2,null,{}, [], [1,2,3]],
                    bar: undefined
                }, 3, "Hey!", "Κάνε πάντα γνωρίζουμε ας των, μηχανής επιδιόρθωσης επιδιορθώσεις ώς μια. Κλπ ας"
            ],
            unicode: "老 汉语中存在 港澳和海外的华人圈中 贵州 我去了书店 现在尚有争",
            b: "b",
            c: fn1
        };

        var same2 = {
            a: [
                "string", null, 0, "1", 1, {
                    prop: null,
                    foo: [1,2,null,{}, [], [1,2,3]],
                    bar: undefined
                }, 3, "Hey!", "Κάνε πάντα γνωρίζουμε ας των, μηχανής επιδιόρθωσης επιδιορθώσεις ώς μια. Κλπ ας"
            ],
            unicode: "老 汉语中存在 港澳和海外的华人圈中 贵州 我去了书店 现在尚有争",
            b: "b",
            c: fn1
        };

        var diff1 = {
            a: [
                "string", null, 0, "1", 1, {
                    prop: null,
                    foo: [1,2,null,{}, [], [1,2,3,4]], // different: 4 was add to the array
                    bar: undefined
                }, 3, "Hey!", "Κάνε πάντα γνωρίζουμε ας των, μηχανής επιδιόρθωσης επιδιορθώσεις ώς μια. Κλπ ας"
            ],
            unicode: "老 汉语中存在 港澳和海外的华人圈中 贵州 我去了书店 现在尚有争",
            b: "b",
            c: fn1
        };

        var diff2 = {
            a: [
                "string", null, 0, "1", 1, {
                    prop: null,
                    foo: [1,2,null,{}, [], [1,2,3]],
                    newprop: undefined, // different: newprop was added
                    bar: undefined
                }, 3, "Hey!", "Κάνε πάντα γνωρίζουμε ας των, μηχανής επιδιόρθωσης επιδιορθώσεις ώς μια. Κλπ ας"
            ],
            unicode: "老 汉语中存在 港澳和海外的华人圈中 贵州 我去了书店 现在尚有争",
            b: "b",
            c: fn1
        };

        var diff3 = {
            a: [
                "string", null, 0, "1", 1, {
                    prop: null,
                    foo: [1,2,null,{}, [], [1,2,3]],
                    bar: undefined
                }, 3, "Hey!", "Κάνε πάντα γνωρίζουμε ας των, μηχανής επιδιόρθωσης επιδιορθώσεις ώς μια. Κλπ α" // different: missing last char
            ],
            unicode: "老 汉语中存在 港澳和海外的华人圈中 贵州 我去了书店 现在尚有争",
            b: "b",
            c: fn1
        };

        var diff4 = {
            a: [
                "string", null, 0, "1", 1, {
                    prop: null,
                    foo: [1,2,undefined,{}, [], [1,2,3]], // different: undefined instead of null
                    bar: undefined
                }, 3, "Hey!", "Κάνε πάντα γνωρίζουμε ας των, μηχανής επιδιόρθωσης επιδιορθώσεις ώς μια. Κλπ ας"
            ],
            unicode: "老 汉语中存在 港澳和海外的华人圈中 贵州 我去了书店 现在尚有争",
            b: "b",
            c: fn1
        };

        var diff5 = {
            a: [
                "string", null, 0, "1", 1, {
                    prop: null,
                    foo: [1,2,null,{}, [], [1,2,3]],
                    bat: undefined // different: property name not "bar"
                }, 3, "Hey!", "Κάνε πάντα γνωρίζουμε ας των, μηχανής επιδιόρθωσης επιδιορθώσεις ώς μια. Κλπ ας"
            ],
            unicode: "老 汉语中存在 港澳和海外的华人圈中 贵州 我去了书店 现在尚有争",
            b: "b",
            c: fn1
        };

        compare(qtest_compareInternal(same1, same2), true);
        compare(qtest_compareInternal(same2, same1), true);
        compare(qtest_compareInternal(same2, diff1), false);
        compare(qtest_compareInternal(diff1, same2), false);

        compare(qtest_compareInternal(same1, diff1), false);
        compare(qtest_compareInternal(same1, diff2), false);
        compare(qtest_compareInternal(same1, diff3), false);
        compare(qtest_compareInternal(same1, diff3), false);
        compare(qtest_compareInternal(same1, diff4), false);
        compare(qtest_compareInternal(same1, diff5), false);
        compare(qtest_compareInternal(diff5, diff1), false);
    }


    function test_complex_arrays() {

        function fn() {
        }

        compare(qtest_compareInternal(
                    [1, 2, 3, true, {}, null, [
                        {
                            a: ["", '1', 0]
                        },
                        5, 6, 7
                    ], "foo"],
                    [1, 2, 3, true, {}, null, [
                        {
                            a: ["", '1', 0]
                        },
                        5, 6, 7
                    ], "foo"]),
                true);

        compare(qtest_compareInternal(
                    [1, 2, 3, true, {}, null, [
                        {
                            a: ["", '1', 0]
                        },
                        5, 6, 7
                    ], "foo"],
                    [1, 2, 3, true, {}, null, [
                        {
                            b: ["", '1', 0]         // not same property name
                        },
                        5, 6, 7
                    ], "foo"]),
                false);

        var a = [{
            b: fn,
            c: false,
            "do": "reserved word",
            "for": {
                ar: [3,5,9,"hey!", [], {
                    ar: [1,[
                        3,4,6,9, null, [], []
                    ]],
                    e: fn,
                    f: undefined
                }]
            },
            e: 0.43445
        }, 5, "string", 0, fn, false, null, undefined, 0, [
            4,5,6,7,8,9,11,22,33,44,55,"66", null, [], [[[[[3]]]], "3"], {}, 1/0
        ], [], [[[], "foo", null, {
            n: 1/0,
            z: {
                a: [3,4,5,6,"yep!", undefined, undefined],
                b: {}
            }
        }, {}]]];

        compare(qtest_compareInternal(a,
                [{
                    b: fn,
                    c: false,
                    "do": "reserved word",
                    "for": {
                        ar: [3,5,9,"hey!", [], {
                            ar: [1,[
                                3,4,6,9, null, [], []
                            ]],
                            e: fn,
                            f: undefined
                        }]
                    },
                    e: 0.43445
                }, 5, "string", 0, fn, false, null, undefined, 0, [
                    4,5,6,7,8,9,11,22,33,44,55,"66", null, [], [[[[[3]]]], "3"], {}, 1/0
                ], [], [[[], "foo", null, {
                    n: 1/0,
                    z: {
                        a: [3,4,5,6,"yep!", undefined, undefined],
                        b: {}
                    }
                }, {}]]]), true);

        compare(qtest_compareInternal(a,
                [{
                    b: fn,
                    c: false,
                    "do": "reserved word",
                    "for": {
                        ar: [3,5,9,"hey!", [], {
                            ar: [1,[
                                3,4,6,9, null, [], []
                            ]],
                            e: fn,
                            f: undefined
                        }]
                    },
                    e: 0.43445
                }, 5, "string", 0, fn, false, null, undefined, 0, [
                    4,5,6,7,8,9,11,22,33,44,55,"66", null, [], [[[[[2]]]], "3"], {}, 1/0    // different: [[[[[2]]]]] instead of [[[[[3]]]]]
                ], [], [[[], "foo", null, {
                    n: 1/0,
                    z: {
                        a: [3,4,5,6,"yep!", undefined, undefined],
                        b: {}
                    }
                }, {}]]]), false);

        compare(qtest_compareInternal(a,
                [{
                    b: fn,
                    c: false,
                    "do": "reserved word",
                    "for": {
                        ar: [3,5,9,"hey!", [], {
                            ar: [1,[
                                3,4,6,9, null, [], []
                            ]],
                            e: fn,
                            f: undefined
                        }]
                    },
                    e: 0.43445
                }, 5, "string", 0, fn, false, null, undefined, 0, [
                    4,5,6,7,8,9,11,22,33,44,55,"66", null, [], [[[[[3]]]], "3"], {}, 1/0
                ], [], [[[], "foo", null, {
                    n: -1/0,                                                                // different, -Infinity instead of Infinity
                    z: {
                        a: [3,4,5,6,"yep!", undefined, undefined],
                        b: {}
                    }
                }, {}]]]), false);

        compare(qtest_compareInternal(a,
                [{
                    b: fn,
                    c: false,
                    "do": "reserved word",
                    "for": {
                        ar: [3,5,9,"hey!", [], {
                            ar: [1,[
                                3,4,6,9, null, [], []
                            ]],
                            e: fn,
                            f: undefined
                        }]
                    },
                    e: 0.43445
                }, 5, "string", 0, fn, false, null, undefined, 0, [
                    4,5,6,7,8,9,11,22,33,44,55,"66", null, [], [[[[[3]]]], "3"], {}, 1/0
                ], [], [[[], "foo", {                                                       // different: null is missing
                    n: 1/0,
                    z: {
                        a: [3,4,5,6,"yep!", undefined, undefined],
                        b: {}
                    }
                }, {}]]]), false);

        compare(qtest_compareInternal(a,
                [{
                    b: fn,
                    c: false,
                    "do": "reserved word",
                    "for": {
                        ar: [3,5,9,"hey!", [], {
                            ar: [1,[
                                3,4,6,9, null, [], []
                            ]],
                            e: fn
                                                                                    // different: missing property f: undefined
                        }]
                    },
                    e: 0.43445
                }, 5, "string", 0, fn, false, null, undefined, 0, [
                    4,5,6,7,8,9,11,22,33,44,55,"66", null, [], [[[[[3]]]], "3"], {}, 1/0
                ], [], [[[], "foo", null, {
                    n: 1/0,
                    z: {
                        a: [3,4,5,6,"yep!", undefined, undefined],
                        b: {}
                    }
                }, {}]]]), false);
    }


    function test_prototypal_inheritance() {
        function Gizmo(id) {
            this.id = id;
        }

        function Hoozit(id) {
            this.id = id;
        }
        Hoozit.prototype = new Gizmo();

        var gizmo = new Gizmo("ok");
        var hoozit = new Hoozit("ok");

        // Try this test many times after test on instances that hold function
        // to make sure that our code does not mess with last object constructor memoization.
        compare(qtest_compareInternal(function () {}, function () {}), false);

        // Hoozit inherit from Gizmo
        // hoozit instanceof Hoozit; // true
        // hoozit instanceof Gizmo; // true
        compare(qtest_compareInternal(hoozit, gizmo), true);

        Gizmo.prototype.bar = true; // not a function just in case we skip them

        // Hoozit inherit from Gizmo
        // They are equivalent
        compare(qtest_compareInternal(hoozit, gizmo), true);

        // Make sure this is still true !important
        // The reason for this is that I forgot to reset the last
        // caller to where it were called from.
        compare(qtest_compareInternal(function () {}, function () {}), false);

        // Make sure this is still true !important
        compare(qtest_compareInternal(hoozit, gizmo), true);

        Hoozit.prototype.foo = true; // not a function just in case we skip them

        // Gizmo does not inherit from Hoozit
        // gizmo instanceof Gizmo; // true
        // gizmo instanceof Hoozit; // false
        // They are not equivalent
        compare(qtest_compareInternal(hoozit, gizmo), false);

        // Make sure this is still true !important
        compare(qtest_compareInternal(function () {}, function () {}), false);
    }


    function test_instances() {
        function A() {}
        var a1 = new A();
        var a2 = new A();

        function B() {
            this.fn = function () {};
        }
        var b1 = new B();
        var b2 = new B();

        compare(qtest_compareInternal(a1, a2), true, "Same property, same constructor");

        // b1.fn and b2.fn are functions but they are different references
        // But we decided to skip function for instances.
        expectFail("", "Don't know if we want to take over function checking like in QUnit")
        compare(qtest_compareInternal(b1, b2), true, "Same property, same constructor");
        compare(qtest_compareInternal(a1, b1), false, "Same properties but different constructor"); // failed

        function Car(year) {
            var privateVar = 0;
            this.year = year;
            this.isOld = function() {
                return year > 10;
            };
        }

        function Human(year) {
            var privateVar = 1;
            this.year = year;
            this.isOld = function() {
                return year > 80;
            };
        }

        var car = new Car(30);
        var carSame = new Car(30);
        var carDiff = new Car(10);
        var human = new Human(30);

        var diff = {
            year: 30
        };

        var same = {
            year: 30,
            isOld: function () {}
        };

        compare(qtest_compareInternal(car, car), true);
        compare(qtest_compareInternal(car, carDiff), false);
        compare(qtest_compareInternal(car, carSame), true);
        compare(qtest_compareInternal(car, human), false);
    }


    function test_complex_instances_nesting() {
    //"Complex Instances Nesting (with function value in literals and/or in nested instances)"
        function A(fn) {
            this.a = {};
            this.fn = fn;
            this.b = {a: []};
            this.o = {};
            this.fn1 = fn;
        }
        function B(fn) {
            this.fn = fn;
            this.fn1 = function () {};
            this.a = new A(function () {});
        }

        function fnOutside() {
        }

        function C(fn) {
            function fnInside() {
            }
            this.x = 10;
            this.fn = fn;
            this.fn1 = function () {};
            this.fn2 = fnInside;
            this.fn3 = {
                a: true,
                b: fnOutside // ok make reference to a function in all instances scope
            };
            this.o1 = {};

            // This function will be ignored.
            // Even if it is not visible for all instances (e.g. locked in a closures),
            // it is from a  property that makes part of an instance (e.g. from the C constructor)
            this.b1 = new B(function () {});
            this.b2 = new B({
                x: {
                    b2: new B(function() {})
                }
            });
        }

        function D(fn) {
            function fnInside() {
            }
            this.x = 10;
            this.fn = fn;
            this.fn1 = function () {};
            this.fn2 = fnInside;
            this.fn3 = {
                a: true,
                b: fnOutside, // ok make reference to a function in all instances scope

                // This function won't be ingored.
                // It isn't visible for all C insances
                // and it is not in a property of an instance. (in an Object instances e.g. the object literal)
                c: fnInside
            };
            this.o1 = {};

            // This function will be ignored.
            // Even if it is not visible for all instances (e.g. locked in a closures),
            // it is from a  property that makes part of an instance (e.g. from the C constructor)
            this.b1 = new B(function () {});
            this.b2 = new B({
                x: {
                    b2: new B(function() {})
                }
            });
        }

        function E(fn) {
            function fnInside() {
            }
            this.x = 10;
            this.fn = fn;
            this.fn1 = function () {};
            this.fn2 = fnInside;
            this.fn3 = {
                a: true,
                b: fnOutside // ok make reference to a function in all instances scope
            };
            this.o1 = {};

            // This function will be ignored.
            // Even if it is not visible for all instances (e.g. locked in a closures),
            // it is from a  property that makes part of an instance (e.g. from the C constructor)
            this.b1 = new B(function () {});
            this.b2 = new B({
                x: {
                    b1: new B({a: function() {}}),
                    b2: new B(function() {})
                }
            });
        }


        var a1 = new A(function () {});
        var a2 = new A(function () {});
        expectFail("", "Don't know if we want to take over function checking like in QUnit")
        compare(qtest_compareInternal(a1, a2), true);

        compare(qtest_compareInternal(a1, a2), true); // different instances

        var b1 = new B(function () {});
        var b2 = new B(function () {});
        compare(qtest_compareInternal(a1, a2), true);

        var c1 = new C(function () {});
        var c2 = new C(function () {});
        compare(qtest_compareInternal(c1, c2), true);

        var d1 = new D(function () {});
        var d2 = new D(function () {});
        compare(qtest_compareInternal(d1, d2), false);

        var e1 = new E(function () {});
        var e2 = new E(function () {});
        compare(qtest_compareInternal(e1, e2), false);

    }
}
