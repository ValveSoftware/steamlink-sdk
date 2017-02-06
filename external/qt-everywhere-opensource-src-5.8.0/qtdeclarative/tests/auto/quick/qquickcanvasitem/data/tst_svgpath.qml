import QtQuick 2.0

CanvasTestCase {
   id:testCase
   name: "svgpath"
   function init_data() { return testData("2d"); }
   function test_svgpath(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       var svgs = [
                   // Absolute coordinates, explicit commands.
                    "M50 0 V50 H0 Q0 25 25 25 T50 0 C25 0 50 50 25 50 S25 0 0 0 Z",
                   // Absolute coordinates, implicit commands.
                    "M50 0 50 50 0 50 Q0 25 25 25 Q50 25 50 0 C25 0 50 50 25 50 C0 50 25 0 0 0 Z",
                   // Relative coordinates, explicit commands.
                    "m50 0 v50 h-50 q0 -25 25 -25 t25 -25 c-25 0 0 50 -25 50 s0 -50 -25 -50 z",
                   // Relative coordinates, implicit commands.
                    "m50 0 0 50 -50 0 q0 -25 25 -25 25 0 25 -25 c-25 0 0 50 -25 50 -25 0 0 -50 -25 -50 z",
                   // Absolute coordinates, explicit commands, minimal whitespace.
                    "m50 0v50h-50q0-25 25-25t25-25c-25 0 0 50-25 50s0-50-25-50z",
                   // Absolute coordinates, explicit commands, extra whitespace.
                    " M  50  0  V  50  H  0  Q 0  25   25 25 T  50 0 C 25   0 50  50 25 50 S  25 0 0  0 Z"
                  ];

       var blues = [
                   -1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    1, 1, 0, 0, 0, 0, 0, 0,-1, 1,
                    1, 1, 1, 0, 0, 0, 0, 0, 1, 1,
                    1, 1, 1, 0, 0, 0, 0, 0, 1, 1,
                    1, 1, 1, 0, 0, 0, 0, 0, 1, 0,
                    1, 1, 1, 0,-1, 1, 1, 1, 0, 0,
                    1, 1, 0, 1, 1, 1, 1, 1, 0, 0,
                    1, 0, 0, 1, 1, 1, 1, 1, 0, 0,
                    1, 0, 0, 1, 1, 1, 1, 1, 0, 0,
                   -1, 0, 0, 0, 1, 1, 1, 0, 0, 0
                   ];

       ctx.fillRule = Qt.OddEvenFill;
       for (var i = 0; i < svgs.length; i++) {
           ctx.fillStyle = "blue";
           ctx.fillRect(0, 0, 50, 50);
           ctx.fillStyle = "red";
           ctx.path = svgs[i];
           ctx.fill();
           var x, y;
           for (x=0; x < 10; x++) {
               for (y=0; y < 10; y++) {
                   if (blues[y*10 +x] == -1) continue; //edge point, different render target may have different value
                   if (blues[y * 10 + x]) {
                       comparePixel(ctx, x * 5, y * 5, 0, 0, 255, 255);
                   } else {
                       comparePixel(ctx, x * 5, y * 5, 255, 0, 0, 255);
                   }
               }
           }
       }
   }
}
