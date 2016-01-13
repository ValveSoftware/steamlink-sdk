import QtQuick 2.0

CanvasTestCase {
   id:testCase
   name: "text"
   function init_data() { return testData("2d"); }
   function test_baseLine(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
  }
   function test_align(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
  }
   function test_stroke(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
  }
   function test_fill(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
  }
   function test_font(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
  }
   function test_measure(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       canvas.destroy()
  }

}
