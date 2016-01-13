import QtQuick 2.0
import "translation.js" as Js

QtObject {
    property string basic: Js.basic()
    property string basic2: Js.basic2()
    property string basic3: Js.basic3()

    property string disambiguation: Js.disambiguation()
    property string disambiguation2: Js.disambiguation2()
    property string disambiguation3: Js.disambiguation3()

    property string noop: Js.noop()
    property string noop2: Js.noop2()

    property string singular: Js.singular()
    property string singular2: Js.singular2()
    property string plural: Js.plural()
    property string plural2: Js.plural2()
}
