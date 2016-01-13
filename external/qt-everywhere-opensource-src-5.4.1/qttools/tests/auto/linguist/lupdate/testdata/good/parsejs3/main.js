({ foo: 123, }) // Trailing comma shouldn't cause syntax error and bailout

({ foo: qsTr("one") })

({ foo: qsTr("two"), })

({ foo: qsTranslate("FooContext", "one") })

({ foo: qsTranslate("FooContext", "two"), })

({ foo: qsTrId("qtn_foo_bar") })

({ foo: qsTrId("qtn_bar_baz"), })

({ foo: { bar: 123, }, baz: qsTr("three"), })

({ foo: { bar: 123, }, baz: qsTranslate("FooContext", "three"), })

({
    firstGuy: {
        age: 50,
        //: First guy first name
        firstName: qsTr("Frits"),
        //: First guy middle name
        middleName: qsTranslate("BarContext", "Joe"),
        //% "First guy last name"
        lastName: qsTrId("qtn_first_guy_last_name"),
        weight: 100, // Uh-oh, trailing comma!
    },
    secondGuy: {
        age: 70,
        //: Second guy first name
        firstName: qsTr("Bob"),
        //: Second guy middle name
        middleName: qsTranslate("BarContext", "Steve"),
        //% "Second guy last name"
        lastName: qsTrId("qtn_second_guy_last_name"),
        weight: 120, // Uh-oh, trailing comma!
    },
})
