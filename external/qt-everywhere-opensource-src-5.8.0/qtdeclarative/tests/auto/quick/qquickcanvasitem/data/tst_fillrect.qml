import QtQuick 2.0

CanvasTestCase {
   id:testCase
   name: "fillRect"
   function init_data() { return testData("2d"); }
   function test_fillRect(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.fillStyle = "red";
       ctx.fillRect(0, 0, canvas.width, canvas.height);
       var imageData = ctx.getImageData(0, 0, 1, 1);
       var d = imageData.data;
       compare(d.length, 4);
       compare(d[0], 255);
       compare(d[1], 0);
       compare(d[2], 0);
       compare(d[3], 255);
  }

}
