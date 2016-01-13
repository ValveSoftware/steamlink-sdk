import QtQuick 2.0

CanvasTestCase {
   id:testCase
   name: "line"
   function init_data() { return testData("2d"); }
   function test_default(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       compare(ctx.lineWidth, 1);
       compare(ctx.lineCap, 'butt');
       compare(ctx.lineJoin, 'miter');
       compare(ctx.miterLimit, 10);
   }

   function test_cross(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       ctx.lineWidth = 200;
       ctx.lineJoin = 'bevel';

       ctx.strokeStyle = '#f00';
       ctx.beginPath();
       ctx.moveTo(110, 50);
       ctx.lineTo(110, 60);
       ctx.lineTo(100, 60);
       ctx.stroke();

       comparePixel(ctx, 1,1, 0,255,0,255);
       comparePixel(ctx, 48,1, 0,255,0,255);
       comparePixel(ctx, 1,48, 0,255,0,255);

       if (canvas.renderTarget === Canvas.Image) {
           //FIXME: broken for Canvas.FramebufferObject
            comparePixel(ctx, 48,48, 0,255,0,255);
       }
   }

   function test_join(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       var tol = 1; // tolerance to avoid antialiasing artifacts

       ctx.lineJoin = 'bevel';
       ctx.lineWidth = 20;

       ctx.fillStyle = '#f00';
       ctx.strokeStyle = '#0f0';

       ctx.fillRect(10, 10, 20, 20);
       ctx.fillRect(20, 20, 20, 20);
       ctx.beginPath();
       ctx.moveTo(30, 20);
       ctx.lineTo(40-tol, 20);
       ctx.lineTo(30, 10+tol);
       ctx.fill();

       ctx.beginPath();
       ctx.moveTo(10, 20);
       ctx.lineTo(30, 20);
       ctx.lineTo(30, 40);
       ctx.stroke();


       ctx.fillStyle = '#0f0';
       ctx.strokeStyle = '#f00';

       ctx.beginPath();
       ctx.moveTo(60, 20);
       ctx.lineTo(80, 20);
       ctx.lineTo(80, 40);
       ctx.stroke();

       ctx.fillRect(60, 10, 20, 20);
       ctx.fillRect(70, 20, 20, 20);
       ctx.beginPath();
       ctx.moveTo(80, 20);
       ctx.lineTo(90+tol, 20);
       ctx.lineTo(80, 10-tol);
       ctx.fill();

       comparePixel(ctx, 34,16, 0,255,0,255);
       comparePixel(ctx, 34,15, 0,255,0,255);
       comparePixel(ctx, 35,15, 0,255,0,255);
       comparePixel(ctx, 36,15, 0,255,0,255);
       comparePixel(ctx, 36,14, 0,255,0,255);

       comparePixel(ctx, 84,16, 0,255,0,255);
       comparePixel(ctx, 84,15, 0,255,0,255);
       comparePixel(ctx, 85,15, 0,255,0,255);
       comparePixel(ctx, 86,15, 0,255,0,255);
       comparePixel(ctx, 86,14, 0,255,0,255);


       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.strokeStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       ctx.lineJoin = 'miter';
       ctx.lineWidth = 200;

       ctx.beginPath();
       ctx.moveTo(100, 50);
       ctx.lineTo(100, 1000);
       ctx.lineTo(1000, 1000);
       ctx.lineTo(1000, 50);
       ctx.closePath();
       ctx.stroke();

       comparePixel(ctx, 1,1, 0,255,0,255);
       comparePixel(ctx, 48,1, 0,255,0,255);
       comparePixel(ctx, 48,48, 0,255,0,255);
       comparePixel(ctx, 1,48, 0,255,0,255);


       ctx.reset();
       ctx.lineJoin = 'bevel'
       compare(ctx.lineJoin, 'bevel');

       ctx.lineJoin = 'bevel';
       ctx.lineJoin = 'invalid';
       compare(ctx.lineJoin, 'bevel');

       ctx.lineJoin = 'bevel';
       ctx.lineJoin = 'ROUND';
       compare(ctx.lineJoin, 'bevel');

       ctx.lineJoin = 'bevel';
       ctx.lineJoin = 'round\\0';
       compare(ctx.lineJoin, 'bevel');

       ctx.lineJoin = 'bevel';
       ctx.lineJoin = 'round ';
       compare(ctx.lineJoin, 'bevel');

       ctx.lineJoin = 'bevel';
       ctx.lineJoin = "";
       compare(ctx.lineJoin, 'bevel');

       ctx.lineJoin = 'bevel';
       ctx.lineJoin = 'butt';
       compare(ctx.lineJoin, 'bevel');

       ctx.reset();
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       ctx.lineJoin = 'miter';
       ctx.lineWidth = 20;

       ctx.fillStyle = '#f00';
       ctx.strokeStyle = '#0f0';

       ctx.fillStyle = '#f00';
       ctx.strokeStyle = '#0f0';

       ctx.fillRect(10, 10, 30, 20);
       ctx.fillRect(20, 10, 20, 30);

       ctx.beginPath();
       ctx.moveTo(10, 20);
       ctx.lineTo(30, 20);
       ctx.lineTo(30, 40);
       ctx.stroke();


       ctx.fillStyle = '#0f0';
       ctx.strokeStyle = '#f00';

       ctx.beginPath();
       ctx.moveTo(60, 20);
       ctx.lineTo(80, 20);
       ctx.lineTo(80, 40);
       ctx.stroke();

       ctx.fillRect(60, 10, 30, 20);
       ctx.fillRect(70, 10, 20, 30);

       comparePixel(ctx, 38,12, 0,255,0,255);
       comparePixel(ctx, 39,11, 0,255,0,255);
       comparePixel(ctx, 40,10, 0,255,0,255);
       comparePixel(ctx, 41,9, 0,255,0,255);
       comparePixel(ctx, 42,8, 0,255,0,255);

       comparePixel(ctx, 88,12, 0,255,0,255);
       comparePixel(ctx, 89,11, 0,255,0,255);
       comparePixel(ctx, 90,10, 0,255,0,255);
       comparePixel(ctx, 91,9, 0,255,0,255);
       comparePixel(ctx, 92,8, 0,255,0,255);

       ctx.reset();
       ctx.fillStyle = '#0f0';
       ctx.strokeStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.lineJoin = 'miter';
       ctx.lineWidth = 200;

       ctx.beginPath();
       ctx.moveTo(100, 50);
       ctx.lineTo(100, 1000);
       ctx.lineTo(1000, 1000);
       ctx.lineTo(1000, 50);
       ctx.lineTo(100, 50);
       ctx.stroke();

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 48,1, 0,255,0,255);
       //comparePixel(ctx, 48,48, 0,255,0,255);
       //comparePixel(ctx, 1,48, 0,255,0,255);

       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.strokeStyle = '#0f0';
       ctx.lineWidth = 300;
       ctx.lineJoin = 'round';
       ctx.beginPath();
       ctx.moveTo(-100, 25);
       ctx.lineTo(0, 25);
       ctx.lineTo(-100, 25);
       ctx.stroke();

       comparePixel(ctx, 1,1, 0,255,0,255);
       comparePixel(ctx, 48,1, 0,255,0,255);
       comparePixel(ctx, 48,48, 0,255,0,255);
       comparePixel(ctx, 1,48, 0,255,0,255);

       ctx.reset();

       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       var tol = 1; // tolerance to avoid antialiasing artifacts

       ctx.lineJoin = 'round';
       ctx.lineWidth = 20;

       ctx.fillStyle = '#f00';
       ctx.strokeStyle = '#0f0';

       ctx.fillRect(10, 10, 20, 20);
       ctx.fillRect(20, 20, 20, 20);
       ctx.beginPath();
       ctx.moveTo(30, 20);
       ctx.arc(30, 20, 10-tol, 0, 2*Math.PI, true);
       ctx.fill();

       ctx.beginPath();
       ctx.moveTo(10, 20);
       ctx.lineTo(30, 20);
       ctx.lineTo(30, 40);
       ctx.stroke();


       ctx.fillStyle = '#0f0';
       ctx.strokeStyle = '#f00';

       ctx.beginPath();
       ctx.moveTo(60, 20);
       ctx.lineTo(80, 20);
       ctx.lineTo(80, 40);
       ctx.stroke();

       ctx.fillRect(60, 10, 20, 20);
       ctx.fillRect(70, 20, 20, 20);
       ctx.beginPath();
       ctx.moveTo(80, 20);
       ctx.arc(80, 20, 10+tol, 0, 2*Math.PI, true);
       ctx.fill();

       comparePixel(ctx, 36,14, 0,255,0,255);
       comparePixel(ctx, 36,13, 0,255,0,255);
       comparePixel(ctx, 37,13, 0,255,0,255);
       comparePixel(ctx, 38,13, 0,255,0,255);
       comparePixel(ctx, 38,12, 0,255,0,255);

       comparePixel(ctx, 86,14, 0,255,0,255);
       comparePixel(ctx, 86,13, 0,255,0,255);
       comparePixel(ctx, 87,13, 0,255,0,255);
       comparePixel(ctx, 88,13, 0,255,0,255);
       comparePixel(ctx, 88,12, 0,255,0,255);

       ctx.reset();
       ctx.lineJoin = 'bevel'
       compare(ctx.lineJoin, 'bevel');

       ctx.lineJoin = 'round';
       compare(ctx.lineJoin, 'round');

       ctx.lineJoin = 'miter';
       compare(ctx.lineJoin, 'miter');

   }
   function test_miter(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.lineWidth = 200;
       ctx.lineJoin = 'miter';

       ctx.strokeStyle = '#0f0';
       ctx.miterLimit = 2.614;
       ctx.beginPath();
       ctx.moveTo(100, 1000);
       ctx.lineTo(100, 100);
       ctx.lineTo(1000, 1000);
       ctx.stroke();

       ctx.strokeStyle = '#f00';
       ctx.miterLimit = 2.613;
       ctx.beginPath();
       ctx.moveTo(100, 1000);
       ctx.lineTo(100, 100);
       ctx.lineTo(1000, 1000);
       ctx.stroke();

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 48,1, 0,255,0,255);
       //comparePixel(ctx, 48,48, 0,255,0,255);
       //comparePixel(ctx, 1,48, 0,255,0,255);

       ctx.reset();
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       ctx.lineWidth = 400;
       ctx.lineJoin = 'miter';

       ctx.strokeStyle = '#f00';
       ctx.miterLimit = 1.414;
       ctx.beginPath();
       ctx.moveTo(200, 1000);
       ctx.lineTo(200, 200);
       ctx.lineTo(1000, 201); // slightly non-right-angle to avoid being a special case
       ctx.stroke();

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 48,1, 0,255,0,255);
       //comparePixel(ctx, 48,48, 0,255,0,255);
       //comparePixel(ctx, 1,48, 0,255,0,255);

       ctx.reset();
       ctx.miterLimit = 1.5;
       compare(ctx.miterLimit, 1.5);

       ctx.miterLimit = 1.5;
       ctx.miterLimit = 0;
       compare(ctx.miterLimit, 1.5);

       ctx.miterLimit = 1.5;
       ctx.miterLimit = -1;
       compare(ctx.miterLimit, 1.5);

       ctx.miterLimit = 1.5;
       ctx.miterLimit = Infinity;
       compare(ctx.miterLimit, 1.5);

       ctx.miterLimit = 1.5;
       ctx.miterLimit = -Infinity;
       compare(ctx.miterLimit, 1.5);

       ctx.miterLimit = 1.5;
       ctx.miterLimit = NaN;
       compare(ctx.miterLimit, 1.5);

       ctx.reset();
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       ctx.lineWidth = 200;
       ctx.lineJoin = 'miter';

       ctx.strokeStyle = '#f00';
       ctx.miterLimit = 1.414;
       ctx.beginPath();
       ctx.strokeRect(100, 25, 200, 0);

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 48,1, 0,255,0,255);
       //comparePixel(ctx, 48,48, 0,255,0,255);
       //comparePixel(ctx, 1,48, 0,255,0,255);

       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.lineWidth = 1600;
       ctx.lineJoin = 'miter';

       ctx.strokeStyle = '#0f0';
       ctx.miterLimit = 1.083;
       ctx.beginPath();
       ctx.moveTo(800, 10000);
       ctx.lineTo(800, 300);
       ctx.lineTo(10000, -8900);
       ctx.stroke();

       ctx.strokeStyle = '#f00';
       ctx.miterLimit = 1.082;
       ctx.beginPath();
       ctx.moveTo(800, 10000);
       ctx.lineTo(800, 300);
       ctx.lineTo(10000, -8900);
       ctx.stroke();

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 48,1, 0,255,0,255);
       //comparePixel(ctx, 48,48, 0,255,0,255);
       //comparePixel(ctx, 1,48, 0,255,0,255);

       ctx.reset();
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       ctx.lineWidth = 400;
       ctx.lineJoin = 'miter';

       ctx.strokeStyle = '#f00';
       ctx.miterLimit = 1.414;
       ctx.beginPath();
       ctx.moveTo(200, 1000);
       ctx.lineTo(200, 200);
       ctx.lineTo(1000, 200);
       ctx.stroke();

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 48,1, 0,255,0,255);
       //comparePixel(ctx, 48,48, 0,255,0,255);
       //comparePixel(ctx, 1,48, 0,255,0,255);

       ctx.reset();
       ctx.miterLimit = 1.5;
       compare(ctx.miterLimit, 1.5);

       ctx.miterLimit = "1e1";
       compare(ctx.miterLimit, 10);

       ctx.miterLimit = 1/1024;
       compare(ctx.miterLimit, 1/1024);

       ctx.miterLimit = 1000;
       compare(ctx.miterLimit, 1000);

       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.lineWidth = 400;
       ctx.lineJoin = 'miter';

       ctx.strokeStyle = '#0f0';
       ctx.miterLimit = 1.416;
       ctx.beginPath();
       ctx.moveTo(200, 1000);
       ctx.lineTo(200, 200);
       ctx.lineTo(1000, 201);
       ctx.stroke();

       comparePixel(ctx, 1,1, 0,255,0,255);
       comparePixel(ctx, 48,1, 0,255,0,255);
       comparePixel(ctx, 48,48, 0,255,0,255);
       comparePixel(ctx, 1,48, 0,255,0,255);


   }
   function test_width(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       ctx.lineWidth = 20;
       // Draw a green line over a red box, to check the line is not too small
       ctx.fillStyle = '#f00';
       ctx.strokeStyle = '#0f0';
       ctx.fillRect(15, 15, 20, 20);
       ctx.beginPath();
       ctx.moveTo(25, 15);
       ctx.lineTo(25, 35);
       ctx.stroke();

       // Draw a green box over a red line, to check the line is not too large
       ctx.fillStyle = '#0f0';
       ctx.strokeStyle = '#f00';
       ctx.beginPath();
       ctx.moveTo(75, 15);
       ctx.lineTo(75, 35);
       ctx.stroke();
       ctx.fillRect(65, 15, 20, 20);

       comparePixel(ctx, 14,25, 0,255,0,255);
       comparePixel(ctx, 15,25, 0,255,0,255);
       comparePixel(ctx, 16,25, 0,255,0,255);
       comparePixel(ctx, 25,25, 0,255,0,255);
       comparePixel(ctx, 34,25, 0,255,0,255);
       comparePixel(ctx, 35,25, 0,255,0,255);
       comparePixel(ctx, 36,25, 0,255,0,255);

       comparePixel(ctx, 64,25, 0,255,0,255);
       comparePixel(ctx, 65,25, 0,255,0,255);
       comparePixel(ctx, 66,25, 0,255,0,255);
       comparePixel(ctx, 75,25, 0,255,0,255);
       comparePixel(ctx, 84,25, 0,255,0,255);
       comparePixel(ctx, 85,25, 0,255,0,255);
       comparePixel(ctx, 86,25, 0,255,0,255);

       ctx.reset();
       ctx.lineWidth = 1.5;
       compare(ctx.lineWidth, 1.5);

       ctx.lineWidth = 1.5;
       ctx.lineWidth = 0;
       compare(ctx.lineWidth, 1.5);

       ctx.lineWidth = 1.5;
       ctx.lineWidth = -1;
       compare(ctx.lineWidth, 1.5);

       ctx.lineWidth = 1.5;
       ctx.lineWidth = Infinity;
       compare(ctx.lineWidth, 1.5);

       ctx.lineWidth = 1.5;
       ctx.lineWidth = -Infinity;
       compare(ctx.lineWidth, 1.5);

       ctx.lineWidth = 1.5;
       ctx.lineWidth = NaN;
       compare(ctx.lineWidth, 1.5);

       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.scale(50, 50);
       ctx.strokeStyle = '#0f0';
       ctx.moveTo(0, 0.5);
       ctx.lineTo(2, 0.5);
       ctx.stroke();

       //comparePixel(ctx, 25,25, 0,255,0,255);
       //comparePixel(ctx, 50,25, 0,255,0,255);
       //comparePixel(ctx, 75,25, 0,255,0,255);
       //comparePixel(ctx, 50,5, 0,255,0,255);
       //comparePixel(ctx, 50,45, 0,255,0,255);

       ctx.reset();
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       ctx.lineWidth = 4;
       // Draw a green line over a red box, to check the line is not too small
       ctx.fillStyle = '#f00';
       ctx.strokeStyle = '#0f0';
       ctx.fillRect(15, 15, 20, 20);
       ctx.save();
        ctx.scale(5, 1);
        ctx.beginPath();
        ctx.moveTo(5, 15);
        ctx.lineTo(5, 35);
        ctx.stroke();
       ctx.restore();

       // Draw a green box over a red line, to check the line is not too large
       ctx.fillStyle = '#0f0';
       ctx.strokeStyle = '#f00';
       ctx.save();
        ctx.scale(-5, 1);
        ctx.beginPath();
        ctx.moveTo(-15, 15);
        ctx.lineTo(-15, 35);
        ctx.stroke();
       ctx.restore();
       ctx.fillRect(65, 15, 20, 20);

       comparePixel(ctx, 14,25, 0,255,0,255);
       //comparePixel(ctx, 15,25, 0,255,0,255);
       //comparePixel(ctx, 16,25, 0,255,0,255);
       //comparePixel(ctx, 25,25, 0,255,0,255);
       //comparePixel(ctx, 34,25, 0,255,0,255);
       //comparePixel(ctx, 35,25, 0,255,0,255);
       //comparePixel(ctx, 36,25, 0,255,0,255);

       //comparePixel(ctx, 64,25, 0,255,0,255);
       //comparePixel(ctx, 65,25, 0,255,0,255);
       //comparePixel(ctx, 66,25, 0,255,0,255);
       //comparePixel(ctx, 75,25, 0,255,0,255);
       //comparePixel(ctx, 84,25, 0,255,0,255);
       //comparePixel(ctx, 85,25, 0,255,0,255);
       //comparePixel(ctx, 86,25, 0,255,0,255);

       ctx.reset();
       ctx.lineWidth = 1.5;
       compare(ctx.lineWidth, 1.5);

       ctx.lineWidth = "1e1";
       compare(ctx.lineWidth, 10);

       ctx.lineWidth = 1/1024;
       compare(ctx.lineWidth, 1/1024);

       ctx.lineWidth = 1000;
       compare(ctx.lineWidth, 1000);

   }
   function test_cap(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       ctx.lineCap = 'butt';
       ctx.lineWidth = 20;

       ctx.fillStyle = '#f00';
       ctx.strokeStyle = '#0f0';
       ctx.fillRect(15, 15, 20, 20);
       ctx.beginPath();
       ctx.moveTo(25, 15);
       ctx.lineTo(25, 35);
       ctx.stroke();

       ctx.fillStyle = '#0f0';
       ctx.strokeStyle = '#f00';
       ctx.beginPath();
       ctx.moveTo(75, 15);
       ctx.lineTo(75, 35);
       ctx.stroke();
       ctx.fillRect(65, 15, 20, 20);

       comparePixel(ctx, 25,14, 0,255,0,255);
       comparePixel(ctx, 25,15, 0,255,0,255);
       comparePixel(ctx, 25,16, 0,255,0,255);
       comparePixel(ctx, 25,34, 0,255,0,255);
       comparePixel(ctx, 25,35, 0,255,0,255);
       comparePixel(ctx, 25,36, 0,255,0,255);

       comparePixel(ctx, 75,14, 0,255,0,255);
       comparePixel(ctx, 75,15, 0,255,0,255);
       comparePixel(ctx, 75,16, 0,255,0,255);
       comparePixel(ctx, 75,34, 0,255,0,255);
       comparePixel(ctx, 75,35, 0,255,0,255);
       comparePixel(ctx, 75,36, 0,255,0,255);

       ctx.reset();

       ctx.fillStyle = '#0f0';
       ctx.strokeStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       ctx.lineJoin = 'bevel';
       ctx.lineCap = 'square';
       ctx.lineWidth = 400;

       ctx.beginPath();
       ctx.moveTo(200, 200);
       ctx.lineTo(200, 1000);
       ctx.lineTo(1000, 1000);
       ctx.lineTo(1000, 200);
       ctx.closePath();
       ctx.stroke();

       comparePixel(ctx, 1,1, 0,255,0,255);
       comparePixel(ctx, 48,1, 0,255,0,255);
       comparePixel(ctx, 48,48, 0,255,0,255);
       comparePixel(ctx, 1,48, 0,255,0,255);
       ctx.reset();

       ctx.lineCap = 'butt'
       compare(ctx.lineCap, 'butt');

       ctx.lineCap = 'butt';
       ctx.lineCap = 'invalid';
       compare(ctx.lineCap, 'butt');

       ctx.lineCap = 'butt';
       ctx.lineCap = 'ROUND';
       compare(ctx.lineCap, 'butt');

       ctx.lineCap = 'butt';
       ctx.lineCap = 'round\\0';
       compare(ctx.lineCap, 'butt');

       ctx.lineCap = 'butt';
       ctx.lineCap = 'round ';
       compare(ctx.lineCap, 'butt');

       ctx.lineCap = 'butt';
       ctx.lineCap = "";
       compare(ctx.lineCap, 'butt');

       ctx.lineCap = 'butt';
       ctx.lineCap = 'bevel';
       compare(ctx.lineCap, 'butt');

       ctx.fillStyle = '#f00';
       ctx.strokeStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       ctx.lineJoin = 'bevel';
       ctx.lineCap = 'square';
       ctx.lineWidth = 400;

       ctx.beginPath();
       ctx.moveTo(200, 200);
       ctx.lineTo(200, 1000);
       ctx.lineTo(1000, 1000);
       ctx.lineTo(1000, 200);
       ctx.lineTo(200, 200);
       ctx.stroke();

       //FIXME:!!!
       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 48,1, 0,255,0,255);
       //comparePixel(ctx, 48,48, 0,255,0,255);
       //comparePixel(ctx, 1,48, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       var tol = 1; // tolerance to avoid antialiasing artifacts

       ctx.lineCap = 'round';
       ctx.lineWidth = 20;


       ctx.fillStyle = '#f00';
       ctx.strokeStyle = '#0f0';

       ctx.beginPath();
       ctx.moveTo(35-tol, 15);
       ctx.arc(25, 15, 10-tol, 0, Math.PI, true);
       ctx.arc(25, 35, 10-tol, Math.PI, 0, true);
       ctx.fill();

       ctx.beginPath();
       ctx.moveTo(25, 15);
       ctx.lineTo(25, 35);
       ctx.stroke();


       ctx.fillStyle = '#0f0';
       ctx.strokeStyle = '#f00';

       ctx.beginPath();
       ctx.moveTo(75, 15);
       ctx.lineTo(75, 35);
       ctx.stroke();

       ctx.beginPath();
       ctx.moveTo(85+tol, 15);
       ctx.arc(75, 15, 10+tol, 0, Math.PI, true);
       ctx.arc(75, 35, 10+tol, Math.PI, 0, true);
       ctx.fill();

       comparePixel(ctx, 17,6, 0,255,0,255);
       comparePixel(ctx, 25,6, 0,255,0,255);
       comparePixel(ctx, 32,6, 0,255,0,255);
       comparePixel(ctx, 17,43, 0,255,0,255);
       comparePixel(ctx, 25,43, 0,255,0,255);
       comparePixel(ctx, 32,43, 0,255,0,255);

       comparePixel(ctx, 67,6, 0,255,0,255);
       comparePixel(ctx, 75,6, 0,255,0,255);
       comparePixel(ctx, 82,6, 0,255,0,255);
       comparePixel(ctx, 67,43, 0,255,0,255);
       comparePixel(ctx, 75,43, 0,255,0,255);
       comparePixel(ctx, 82,43, 0,255,0,255);
       ctx.reset();

       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       ctx.lineCap = 'square';
       ctx.lineWidth = 20;

       ctx.fillStyle = '#f00';
       ctx.strokeStyle = '#0f0';
       ctx.fillRect(15, 5, 20, 40);
       ctx.beginPath();
       ctx.moveTo(25, 15);
       ctx.lineTo(25, 35);
       ctx.stroke();

       ctx.fillStyle = '#0f0';
       ctx.strokeStyle = '#f00';
       ctx.beginPath();
       ctx.moveTo(75, 15);
       ctx.lineTo(75, 35);
       ctx.stroke();
       ctx.fillRect(65, 5, 20, 40);

       comparePixel(ctx, 25,4, 0,255,0,255);
       comparePixel(ctx, 25,5, 0,255,0,255);
       comparePixel(ctx, 25,6, 0,255,0,255);
       comparePixel(ctx, 25,44, 0,255,0,255);
       comparePixel(ctx, 25,45, 0,255,0,255);
       comparePixel(ctx, 25,46, 0,255,0,255);

       comparePixel(ctx, 75,4, 0,255,0,255);
       comparePixel(ctx, 75,5, 0,255,0,255);
       comparePixel(ctx, 75,6, 0,255,0,255);
       comparePixel(ctx, 75,44, 0,255,0,255);
       comparePixel(ctx, 75,45, 0,255,0,255);
       comparePixel(ctx, 75,46, 0,255,0,255);

       ctx.reset();
       ctx.lineCap = 'butt'
       compare(ctx.lineCap, 'butt');

       ctx.lineCap = 'round';
       compare(ctx.lineCap, 'round');

       ctx.lineCap = 'square';
       compare(ctx.lineCap, 'square');

  }
}
