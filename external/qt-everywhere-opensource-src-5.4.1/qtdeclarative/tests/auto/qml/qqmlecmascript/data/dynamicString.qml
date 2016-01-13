import Qt.test 1.0
import QtQuick 2.0

MyTypeObject {
  stringProperty:"string:%0 false:%1 true:%2 uint32:%3 int32:%4 double:%5 date:%6!"
  Component.onCompleted: {
     var date = new Date();
     date.setDate(11);
     date.setMonth(1);
     date.setFullYear(2011);
     date.setHours(5);
     date.setMinutes(30);
     date.setSeconds(50);
     stringProperty = stringProperty.arg("Hello World").arg(false).arg(true).arg(100).arg(-100).arg(3.1415926).arg(Qt.formatDateTime(date, "yyyy-MM-dd hh::mm:ss"));
  }
}
