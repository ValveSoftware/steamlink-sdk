import QtQuick 2.0

CanvasTestCase {
   id:testCase
   name: "path"
   function init_data() { return testData("2d"); }
       function test_basic(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.closePath();
           ctx.fillStyle = '#f00';
           ctx.fill();
           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.save();
           ctx.rect(0, 0, 100, 50);
           ctx.restore();
           ctx.fillStyle = '#0f0';
           ctx.fill();
           comparePixel(ctx, 50,25, 0,255,0,255);


           canvas.width = 100;
           ctx.rect(0, 0, 100, 50);
           canvas.width = 100;
           ctx.fillStyle = '#f00';
           ctx.fill();
           //comparePixel(ctx, 20,20, 0,0,0,0);
           canvas.destroy()
       }
       function test_beginPath(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.rect(0, 0, 100, 50);
           ctx.beginPath();
           ctx.fillStyle = '#f00';
           ctx.fill();
           comparePixel(ctx, 50,25, 0,255,0,255);
           canvas.destroy()
       }
       function test_closePath(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.closePath();
           ctx.fillStyle = '#f00';
           ctx.fill();
           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 50;
           ctx.moveTo(-100, 25);
           ctx.lineTo(-100, -100);
           ctx.lineTo(200, -100);
           ctx.lineTo(200, 25);
           ctx.closePath();
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 50;
           ctx.moveTo(-100, 25);
           ctx.lineTo(-100, -1000);
           ctx.closePath();
           ctx.lineTo(1000, 25);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);
           canvas.destroy()
       }

       function test_isPointInPath(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();
           ctx.arc(50, 25, 10, 0, Math.PI, false);
           verify(!ctx.isPointInPath(50, 10));
           verify(!ctx.isPointInPath(50, 20));
           //verify(!ctx.isPointInPath(50, 30));
           verify(!ctx.isPointInPath(50, 40));
           verify(!ctx.isPointInPath(30, 20));
           verify(!ctx.isPointInPath(70, 20));
           verify(!ctx.isPointInPath(30, 30));
           verify(!ctx.isPointInPath(70, 30));

           ctx.reset();
           ctx.rect(0, 0, 20, 20);
           verify(ctx.isPointInPath(10, 10));
           verify(!ctx.isPointInPath(30, 10));

           ctx.reset();
           ctx.rect(20, 0, 20, 20);
           verify(!ctx.isPointInPath(10, 10));
           verify(ctx.isPointInPath(30, 10));

           ctx.reset();
           ctx.bezierCurveTo(50, -50, 50, 100, 75, 25);
           verify(!ctx.isPointInPath(25, 20));
           verify(!ctx.isPointInPath(25, 30));
           //verify(ctx.isPointInPath(30, 20));
           verify(!ctx.isPointInPath(30, 30));
           verify(!ctx.isPointInPath(40, 2));
           //verify(ctx.isPointInPath(40, 20));
           verify(!ctx.isPointInPath(40, 30));
           verify(!ctx.isPointInPath(40, 47));
           //verify(ctx.isPointInPath(45, 20));
           verify(!ctx.isPointInPath(45, 30));
           //verify(!ctx.isPointInPath(55, 20));
           //verify(ctx.isPointInPath(55, 30));
           //verify(!ctx.isPointInPath(60, 2));
           //verify(!ctx.isPointInPath(60, 20));
           verify(ctx.isPointInPath(60, 30));
           verify(!ctx.isPointInPath(60, 47));
           //verify(!ctx.isPointInPath(70, 20));
           verify(ctx.isPointInPath(70, 30));
           verify(!ctx.isPointInPath(75, 20));
           verify(!ctx.isPointInPath(75, 30));

           ctx.reset();
           ctx.arc(50, 25, 10, 0, 7, false);
           verify(!ctx.isPointInPath(50, 10));
           verify(ctx.isPointInPath(50, 20));
           verify(ctx.isPointInPath(50, 30));
           verify(!ctx.isPointInPath(50, 40));
           verify(!ctx.isPointInPath(30, 20));
           verify(!ctx.isPointInPath(70, 20));
           verify(!ctx.isPointInPath(30, 30));
           //verify(!ctx.isPointInPath(70, 30));

           ctx.reset();
           ctx.rect(0, 0, 20, 20);
           verify(ctx.isPointInPath(0, 0));
           verify(ctx.isPointInPath(10, 0));
           verify(ctx.isPointInPath(20, 0));
           verify(ctx.isPointInPath(20, 10));
           verify(ctx.isPointInPath(20, 20));
           verify(ctx.isPointInPath(10, 20));
           verify(ctx.isPointInPath(0, 20));
           verify(ctx.isPointInPath(0, 10));
           verify(!ctx.isPointInPath(10, -0.01));
           verify(!ctx.isPointInPath(10, 20.01));
           verify(!ctx.isPointInPath(-0.01, 10));
           verify(!ctx.isPointInPath(20.01, 10));

           ctx.reset();
           verify(!ctx.isPointInPath(0, 0));


           ctx.reset();
           ctx.rect(-100, -50, 200, 100);
           verify(!ctx.isPointInPath(Infinity, 0));
           verify(!ctx.isPointInPath(-Infinity, 0));
           verify(!ctx.isPointInPath(NaN, 0));
           verify(!ctx.isPointInPath(0, Infinity));
           verify(!ctx.isPointInPath(0, -Infinity));
           verify(!ctx.isPointInPath(0, NaN));
           verify(!ctx.isPointInPath(NaN, NaN));

           ctx.reset();
           ctx.rect(0, -100, 20, 20);
           ctx.rect(20, -10, 20, 20);
           verify(!ctx.isPointInPath(10, -110));
           verify(ctx.isPointInPath(10, -90));
           verify(!ctx.isPointInPath(10, -70));
           verify(!ctx.isPointInPath(30, -20));
           verify(ctx.isPointInPath(30, 0));
           verify(!ctx.isPointInPath(30, 20));

           ctx.reset();
           ctx.rect(0, 0, 20, 20);
           ctx.beginPath();
           ctx.rect(20, 0, 20, 20);
           ctx.closePath();
           ctx.rect(40, 0, 20, 20);
           verify(!ctx.isPointInPath(10, 10));
           verify(ctx.isPointInPath(30, 10));
           verify(ctx.isPointInPath(50, 10));

           ctx.reset();
           ctx.translate(50, 0);
           ctx.rect(0, 0, 20, 20);
           verify(!ctx.isPointInPath(-40, 10));
           verify(!ctx.isPointInPath(10, 10));
           verify(!ctx.isPointInPath(49, 10));
           verify(ctx.isPointInPath(51, 10));
           verify(ctx.isPointInPath(69, 10));
           verify(!ctx.isPointInPath(71, 10));

           ctx.reset();
           ctx.rect(50, 0, 20, 20);
           ctx.translate(50, 0);
           verify(!ctx.isPointInPath(-40, 10));
           verify(!ctx.isPointInPath(10, 10));
           verify(!ctx.isPointInPath(49, 10));
           verify(ctx.isPointInPath(51, 10));
           verify(ctx.isPointInPath(69, 10));
           verify(!ctx.isPointInPath(71, 10));

           ctx.reset();
           ctx.scale(-1, 1);
           ctx.rect(-70, 0, 20, 20);
           verify(!ctx.isPointInPath(-40, 10));
           verify(!ctx.isPointInPath(10, 10));
           verify(!ctx.isPointInPath(49, 10));
           verify(ctx.isPointInPath(51, 10));
           verify(ctx.isPointInPath(69, 10));
           verify(!ctx.isPointInPath(71, 10));

           ctx.reset();
           ctx.moveTo(0, 0);
           ctx.lineTo(20, 0);
           ctx.lineTo(20, 20);
           ctx.lineTo(0, 20);
           verify(ctx.isPointInPath(10, 10));
           verify(!ctx.isPointInPath(30, 10));

           ctx.reset();
           ctx.moveTo(0, 0);
           ctx.lineTo(50, 0);
           ctx.lineTo(50, 50);
           ctx.lineTo(0, 50);
           ctx.lineTo(0, 0);
           ctx.lineTo(10, 10);
           ctx.lineTo(10, 40);
           ctx.lineTo(40, 40);
           ctx.lineTo(40, 10);
           ctx.lineTo(10, 10);

           verify(ctx.isPointInPath(5, 5));
           verify(ctx.isPointInPath(25, 5));
           verify(ctx.isPointInPath(45, 5));
           verify(ctx.isPointInPath(5, 25));
           verify(!ctx.isPointInPath(25, 25));
           verify(ctx.isPointInPath(45, 25));
           verify(ctx.isPointInPath(5, 45));
           verify(ctx.isPointInPath(25, 45));
           verify(ctx.isPointInPath(45, 45));
           canvas.destroy()
       }


       function test_fill(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);

           ctx.fillStyle = '#0f0';
           ctx.moveTo(0, 0);
           ctx.lineTo(100, 0);
           ctx.lineTo(100, 50);
           ctx.lineTo(0, 50);
           ctx.fill();

           comparePixel(ctx, 50,25, 0,255,0,255);


           ctx.reset();
           ctx.fillStyle = '#00f';
           ctx.fillRect(0, 0, 100, 50);

           ctx.moveTo(0, 0);
           ctx.lineTo(100, 0);
           ctx.lineTo(100, 50);
           ctx.fillStyle = '#f00';
           ctx.fill();
           ctx.lineTo(0, 50);
           ctx.fillStyle = '#0f0';
           ctx.fill();

           comparePixel(ctx, 90,10, 0,255,0,255);
           comparePixel(ctx, 10,40, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#000';
           ctx.fillRect(0, 0, 100, 50);

           ctx.fillStyle = 'rgba(0, 255, 0, 0.5)';
           ctx.rect(0, 0, 100, 50);
           ctx.closePath();
           ctx.rect(10, 10, 80, 30);
           ctx.fill();

           comparePixel(ctx, 50,25, 0,127,0,255, 1);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);

           ctx.fillStyle = '#0f0';
           ctx.moveTo(-10, -10);
           ctx.lineTo(110, -10);
           ctx.lineTo(110, 60);
           ctx.lineTo(-10, 60);
           ctx.lineTo(-10, -10);
           ctx.lineTo(0, 0);
           ctx.lineTo(100, 0);
           ctx.lineTo(100, 50);
           ctx.lineTo(0, 50);
           ctx.fill();

           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);

           ctx.fillStyle = '#f00';
           ctx.moveTo(-10, -10);
           ctx.lineTo(110, -10);
           ctx.lineTo(110, 60);
           ctx.lineTo(-10, 60);
           ctx.lineTo(-10, -10);
           ctx.lineTo(0, 0);
           ctx.lineTo(0, 50);
           ctx.lineTo(100, 50);
           ctx.lineTo(100, 0);
           ctx.fill();

           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);

           ctx.fillStyle = '#f00';
           ctx.moveTo(-10, -10);
           ctx.lineTo(110, -10);
           ctx.lineTo(110, 60);
           ctx.lineTo(-10, 60);
           ctx.moveTo(0, 0);
           ctx.lineTo(0, 50);
           ctx.lineTo(100, 50);
           ctx.lineTo(100, 0);
           ctx.fill();

           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);

           ctx.fillStyle = '#0f0';
           ctx.moveTo(-10, -10);
           ctx.lineTo(110, -10);
           ctx.lineTo(110, 60);
           ctx.lineTo(-10, 60);
           ctx.lineTo(-10, -10);
           ctx.lineTo(-20, -20);
           ctx.lineTo(120, -20);
           ctx.lineTo(120, 70);
           ctx.lineTo(-20, 70);
           ctx.lineTo(-20, -20);
           ctx.lineTo(0, 0);
           ctx.lineTo(0, 50);
           ctx.lineTo(100, 50);
           ctx.lineTo(100, 0);
           ctx.fill();

           comparePixel(ctx, 50,25, 0,255,0,255);
           canvas.destroy()
       }
       function test_stroke(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);

           ctx.strokeStyle = '#f00';
           ctx.lineWidth = 100;
           ctx.lineCap = 'round';
           ctx.lineJoin = 'round';

           ctx.beginPath();
           ctx.moveTo(40, 25);
           ctx.moveTo(60, 25);
           ctx.stroke();

           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#000';
           ctx.fillRect(0, 0, 100, 50);

           ctx.strokeStyle = 'rgba(0, 255, 0, 0.5)';
           ctx.lineWidth = 50;
           ctx.moveTo(0, 20);
           ctx.lineTo(100, 20);
           ctx.moveTo(0, 30);
           ctx.lineTo(100, 30);
           ctx.stroke();

           //comparePixel(ctx, 50,25, 0,127,0,255);

           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);

           ctx.strokeStyle = '#f00';
           ctx.lineWidth = 100;
           ctx.lineCap = 'round';
           ctx.lineJoin = 'round';

           ctx.beginPath();
           ctx.moveTo(50, 25);
           ctx.arcTo(50, 25, 150, 25, 10);
           ctx.stroke();

           ctx.beginPath();
           ctx.moveTo(50, 25);
           ctx.arc(50, 25, 10, 0, 0, false);
           ctx.stroke();

           comparePixel(ctx, 50,25, 0,255,0,255);
           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);

           ctx.strokeStyle = '#f00';
           ctx.lineWidth = 100;
           ctx.lineCap = 'round';
           ctx.lineJoin = 'round';

           ctx.beginPath();
           ctx.moveTo(50, 25);
           ctx.lineTo(50, 25);
           ctx.closePath();
           ctx.stroke();

           comparePixel(ctx, 50,25, 0,255,0,255);
           ctx.reset();

           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);

           ctx.strokeStyle = '#f00';
           ctx.lineWidth = 400;
           ctx.lineJoin = 'miter';
           ctx.miterLimit = 1.4;

           ctx.beginPath();
           ctx.moveTo(-1000, 200, 0, 0);
           ctx.lineTo(-100, 200);
           ctx.lineTo(-100, 200);
           ctx.lineTo(-100, 200);
           ctx.lineTo(-100, 1000);
           ctx.stroke();

           //FIXME:lineJoin with miterLimit test fail!
           //comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);

           ctx.strokeStyle = '#f00';
           ctx.lineWidth = 100;
           ctx.lineCap = 'round';
           ctx.lineJoin = 'round';

           ctx.beginPath();
           ctx.moveTo(50, 25);
           ctx.quadraticCurveTo(50, 25, 50, 25);
           ctx.stroke();

           ctx.beginPath();
           ctx.moveTo(50, 25);
           ctx.bezierCurveTo(50, 25, 50, 25, 50, 25);
           ctx.stroke();

           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);

           ctx.strokeStyle = '#f00';
           ctx.lineWidth = 100;
           ctx.lineCap = 'round';
           ctx.lineJoin = 'round';

           ctx.beginPath();
           ctx.moveTo(50, 25);
           ctx.lineTo(50, 25);
           ctx.stroke();

           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);

           ctx.strokeStyle = '#f00';
           ctx.lineWidth = 100;
           ctx.lineCap = 'round';
           ctx.lineJoin = 'round';

           ctx.beginPath();
           ctx.rect(50, 25, 0, 0);
           ctx.stroke();

           ctx.strokeRect(50, 25, 0, 0);

           comparePixel(ctx, 50,25, 0,255,0,255);


           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);

           ctx.beginPath();
           ctx.rect(25, 12.5, 50, 25);
           ctx.save();
           ctx.scale(50, 25);
           ctx.strokeStyle = '#0f0';
           ctx.stroke();
           ctx.restore();

           ctx.beginPath();
           ctx.rect(-25, -12.5, 150, 75);
           ctx.save();
           ctx.scale(50, 25);
           ctx.strokeStyle = '#f00';
           ctx.stroke();
           ctx.restore();

           //comparePixel(ctx, 0,0, 0,255,0,255);
           //comparePixel(ctx, 50,0, 0,255,0,255);
           //comparePixel(ctx, 99,0, 0,255,0,255);
           //comparePixel(ctx, 0,25, 0,255,0,255);
           //comparePixel(ctx, 50,25, 0,255,0,255);
           //comparePixel(ctx, 99,25, 0,255,0,255);
           //comparePixel(ctx, 0,49, 0,255,0,255);
           //comparePixel(ctx, 50,49, 0,255,0,255);
           //comparePixel(ctx, 99,49, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);

           ctx.beginPath();
           ctx.rect(25, 12.5, 50, 25);
           ctx.save();
           ctx.rotate(Math.PI/2);
           ctx.scale(25, 50);
           ctx.strokeStyle = '#0f0';
           ctx.stroke();
           ctx.restore();

           ctx.beginPath();
           ctx.rect(-25, -12.5, 150, 75);
           ctx.save();
           ctx.rotate(Math.PI/2);
           ctx.scale(25, 50);
           ctx.strokeStyle = '#f00';
           ctx.stroke();
           ctx.restore();

           comparePixel(ctx, 0,0, 0,255,0,255);
           comparePixel(ctx, 50,0, 0,255,0,255);
           comparePixel(ctx, 99,0, 0,255,0,255);
           comparePixel(ctx, 0,25, 0,255,0,255);
           comparePixel(ctx, 50,25, 0,255,0,255);
           comparePixel(ctx, 99,25, 0,255,0,255);
           comparePixel(ctx, 0,49, 0,255,0,255);
           comparePixel(ctx, 50,49, 0,255,0,255);
           comparePixel(ctx, 99,49, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);

           ctx.save();
           ctx.beginPath();
           ctx.moveTo(49, -50);
           ctx.lineTo(201, -50);
           ctx.rotate(Math.PI/4);
           ctx.scale(1, 283);
           ctx.strokeStyle = '#0f0';
           ctx.stroke();
           ctx.restore();

           ctx.save();
           ctx.beginPath();
           ctx.translate(-150, 0);
           ctx.moveTo(49, -50);
           ctx.lineTo(199, -50);
           ctx.rotate(Math.PI/4);
           ctx.scale(1, 142);
           ctx.strokeStyle = '#f00';
           ctx.stroke();
           ctx.restore();

           ctx.save();
           ctx.beginPath();
           ctx.translate(-150, 0);
           ctx.moveTo(49, -50);
           ctx.lineTo(199, -50);
           ctx.rotate(Math.PI/4);
           ctx.scale(1, 142);
           ctx.strokeStyle = '#f00';
           ctx.stroke();
           ctx.restore();

           //comparePixel(ctx, 0,0, 0,255,0,255);
           comparePixel(ctx, 50,0, 0,255,0,255);
           //comparePixel(ctx, 99,0, 0,255,0,255);
           //comparePixel(ctx, 0,25, 0,255,0,255);
           comparePixel(ctx, 50,25, 0,255,0,255);
           comparePixel(ctx, 99,25, 0,255,0,255);
           //comparePixel(ctx, 0,49, 0,255,0,255);
           comparePixel(ctx, 50,49, 0,255,0,255);
           comparePixel(ctx, 99,49, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);

           ctx.lineWidth = 50;
           ctx.moveTo(-100, 25);
           ctx.lineTo(-100, -100);
           ctx.lineTo(200, -100);
           ctx.lineTo(200, 25);
           ctx.strokeStyle = '#f00';
           ctx.stroke();

           ctx.closePath();
           ctx.strokeStyle = '#0f0';
           ctx.stroke();

           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);

           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 40;
           ctx.moveTo(0, 10);
           ctx.lineTo(100, 10);
           ctx.moveTo(100, 40);
           ctx.lineTo(0, 40);
           ctx.stroke();

           comparePixel(ctx, 50,25, 0,255,0,255);
           canvas.destroy()
       }
       function test_clip(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);

           ctx.beginPath();
           ctx.rect(0, 0, 100, 50);
           ctx.clip();

           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);

           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);

           ctx.beginPath();
           ctx.rect(-100, 0, 100, 50);
           ctx.clip();

           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);

           comparePixel(ctx, 50,25, 0,255,0,255);
           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);

           ctx.beginPath();
           ctx.clip();

           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);

           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);

           ctx.beginPath();
           ctx.rect(0, 0, 50, 50);
           ctx.clip();
           ctx.beginPath();
           ctx.rect(50, 0, 50, 50)
           ctx.clip();

           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);

           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);

           ctx.fillStyle = '#0f0';

           ctx.beginPath();
           ctx.moveTo(0, 0);
           ctx.lineTo(0, 50);
           ctx.lineTo(100, 50);
           ctx.lineTo(100, 0);
           ctx.clip();

           ctx.lineTo(0, 0);
           ctx.fill();

           comparePixel(ctx, 50,25, 0,255,0,255);


           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);

           ctx.beginPath();
           ctx.moveTo(-10, -10);
           ctx.lineTo(110, -10);
           ctx.lineTo(110, 60);
           ctx.lineTo(-10, 60);
           ctx.lineTo(-10, -10);
           ctx.lineTo(0, 0);
           ctx.lineTo(0, 50);
           ctx.lineTo(100, 50);
           ctx.lineTo(100, 0);
           ctx.clip();

           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);

           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);

           ctx.beginPath();
           ctx.moveTo(-10, -10);
           ctx.lineTo(110, -10);
           ctx.lineTo(110, 60);
           ctx.lineTo(-10, 60);
           ctx.lineTo(-10, -10);
           ctx.clip();

           ctx.beginPath();
           ctx.moveTo(0, 0);
           ctx.lineTo(0, 50);
           ctx.lineTo(100, 50);
           ctx.lineTo(100, 0);
           ctx.lineTo(0, 0);
           ctx.clip();

           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);

           comparePixel(ctx, 50,25, 0,255,0,255);
           canvas.destroy()
       }

       function test_moveTo(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.rect(0, 0, 10, 50);
           ctx.moveTo(100, 0);
           ctx.lineTo(10, 0);
           ctx.lineTo(10, 50);
           ctx.lineTo(100, 50);
           ctx.fillStyle = '#0f0';
           ctx.fill();
           comparePixel(ctx, 90,25, 0,255,0,255);
           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.moveTo(0, 25);
           ctx.moveTo(100, 25);
           ctx.moveTo(0, 25);
           ctx.lineTo(100, 25);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 50;
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.beginPath();
           ctx.moveTo(0, 0);
           ctx.moveTo(100, 0);
           ctx.moveTo(100, 50);
           ctx.moveTo(0, 50);
           ctx.fillStyle = '#f00';
           ctx.fill();
           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.moveTo(0, 0);
           ctx.lineTo(100, 0);
           ctx.moveTo(Infinity, 50);
           ctx.moveTo(-Infinity, 50);
           ctx.moveTo(NaN, 50);
           ctx.moveTo(0, Infinity);
           ctx.moveTo(0, -Infinity);
           ctx.moveTo(0, NaN);
           ctx.moveTo(Infinity, Infinity);
           ctx.lineTo(100, 50);
           ctx.lineTo(0, 50);
           ctx.fillStyle = '#0f0';
           ctx.fill();
           comparePixel(ctx, 50,25, 0,255,0,255);
           canvas.destroy()
       }
       function test_lineTo(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 50;
           ctx.beginPath();
           ctx.moveTo(0, 25);
           ctx.lineTo(100, 25);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#f00';
           ctx.lineWidth = 50;
           ctx.beginPath();
           ctx.lineTo(100, 50);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 50;
           ctx.beginPath();
           ctx.lineTo(0, 25);
           ctx.lineTo(100, 25);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 50;
           ctx.beginPath();
           ctx.moveTo(-100, -100);
           ctx.lineTo(0, 25);
           ctx.lineTo(100, 25);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.moveTo(0, 0);
           ctx.lineTo(100, 0);
           ctx.lineTo(Infinity, 50);
           ctx.lineTo(-Infinity, 50);
           ctx.lineTo(NaN, 50);
           ctx.lineTo(0, Infinity);
           ctx.lineTo(0, -Infinity);
           ctx.lineTo(0, NaN);
           ctx.lineTo(Infinity, Infinity);
           ctx.lineTo(100, 50);
           ctx.lineTo(0, 50);
           ctx.fillStyle = '#0f0';
           ctx.fill();
           comparePixel(ctx, 50,25, 0,255,0,255);
           comparePixel(ctx, 90,45, 0,255,0,255);
           canvas.destroy()
       }
       function test_bezierCurveTo(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 50;
           ctx.beginPath();
           ctx.moveTo(0, 25);
           ctx.bezierCurveTo(100, 25, 100, 25, 100, 25);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#f00';
           ctx.lineWidth = 50;
           ctx.beginPath();
           ctx.bezierCurveTo(100, 50, 200, 50, 200, 50);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);
           comparePixel(ctx, 95,45, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 50;
           ctx.beginPath();
           ctx.bezierCurveTo(0, 25, 100, 25, 100, 25);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);
           comparePixel(ctx, 5,45, 0,255,0,255);

           ctx.reset();
           ctx.moveTo(0, 0);
           ctx.lineTo(100, 0);
           ctx.bezierCurveTo(Infinity, 50, 0, 50, 0, 50);
           ctx.bezierCurveTo(-Infinity, 50, 0, 50, 0, 50);
           ctx.bezierCurveTo(NaN, 50, 0, 50, 0, 50);
           ctx.bezierCurveTo(0, Infinity, 0, 50, 0, 50);
           ctx.bezierCurveTo(0, -Infinity, 0, 50, 0, 50);
           ctx.bezierCurveTo(0, NaN, 0, 50, 0, 50);
           ctx.bezierCurveTo(0, 50, Infinity, 50, 0, 50);
           ctx.bezierCurveTo(0, 50, -Infinity, 50, 0, 50);
           ctx.bezierCurveTo(0, 50, NaN, 50, 0, 50);
           ctx.bezierCurveTo(0, 50, 0, Infinity, 0, 50);
           ctx.bezierCurveTo(0, 50, 0, -Infinity, 0, 50);
           ctx.bezierCurveTo(0, 50, 0, NaN, 0, 50);
           ctx.bezierCurveTo(0, 50, 0, 50, Infinity, 50);
           ctx.bezierCurveTo(0, 50, 0, 50, -Infinity, 50);
           ctx.bezierCurveTo(0, 50, 0, 50, NaN, 50);
           ctx.bezierCurveTo(0, 50, 0, 50, 0, Infinity);
           ctx.bezierCurveTo(0, 50, 0, 50, 0, -Infinity);
           ctx.bezierCurveTo(0, 50, 0, 50, 0, NaN);
           ctx.bezierCurveTo(Infinity, Infinity, 0, 50, 0, 50);
           ctx.bezierCurveTo(Infinity, Infinity, Infinity, 50, 0, 50);
           ctx.bezierCurveTo(Infinity, Infinity, Infinity, Infinity, 0, 50);
           ctx.bezierCurveTo(Infinity, Infinity, Infinity, Infinity, Infinity, 50);
           ctx.bezierCurveTo(Infinity, Infinity, Infinity, Infinity, Infinity, Infinity);
           ctx.bezierCurveTo(Infinity, Infinity, Infinity, Infinity, 0, Infinity);
           ctx.bezierCurveTo(Infinity, Infinity, Infinity, 50, Infinity, 50);
           ctx.bezierCurveTo(Infinity, Infinity, Infinity, 50, Infinity, Infinity);
           ctx.bezierCurveTo(Infinity, Infinity, Infinity, 50, 0, Infinity);
           ctx.bezierCurveTo(Infinity, Infinity, 0, Infinity, 0, 50);
           ctx.bezierCurveTo(Infinity, Infinity, 0, Infinity, Infinity, 50);
           ctx.bezierCurveTo(Infinity, Infinity, 0, Infinity, Infinity, Infinity);
           ctx.bezierCurveTo(Infinity, Infinity, 0, Infinity, 0, Infinity);
           ctx.bezierCurveTo(Infinity, Infinity, 0, 50, Infinity, 50);
           ctx.bezierCurveTo(Infinity, Infinity, 0, 50, Infinity, Infinity);
           ctx.bezierCurveTo(Infinity, Infinity, 0, 50, 0, Infinity);
           ctx.bezierCurveTo(Infinity, 50, Infinity, 50, 0, 50);
           ctx.bezierCurveTo(Infinity, 50, Infinity, Infinity, 0, 50);
           ctx.bezierCurveTo(Infinity, 50, Infinity, Infinity, Infinity, 50);
           ctx.bezierCurveTo(Infinity, 50, Infinity, Infinity, Infinity, Infinity);
           ctx.bezierCurveTo(Infinity, 50, Infinity, Infinity, 0, Infinity);
           ctx.bezierCurveTo(Infinity, 50, Infinity, 50, Infinity, 50);
           ctx.bezierCurveTo(Infinity, 50, Infinity, 50, Infinity, Infinity);
           ctx.bezierCurveTo(Infinity, 50, Infinity, 50, 0, Infinity);
           ctx.bezierCurveTo(Infinity, 50, 0, Infinity, 0, 50);
           ctx.bezierCurveTo(Infinity, 50, 0, Infinity, Infinity, 50);
           ctx.bezierCurveTo(Infinity, 50, 0, Infinity, Infinity, Infinity);
           ctx.bezierCurveTo(Infinity, 50, 0, Infinity, 0, Infinity);
           ctx.bezierCurveTo(Infinity, 50, 0, 50, Infinity, 50);
           ctx.bezierCurveTo(Infinity, 50, 0, 50, Infinity, Infinity);
           ctx.bezierCurveTo(Infinity, 50, 0, 50, 0, Infinity);
           ctx.bezierCurveTo(Infinity, 50, Infinity, 50, 0, 50);
           ctx.bezierCurveTo(Infinity, 50, Infinity, Infinity, 0, 50);
           ctx.bezierCurveTo(Infinity, 50, Infinity, Infinity, Infinity, 50);
           ctx.bezierCurveTo(Infinity, 50, Infinity, Infinity, Infinity, Infinity);
           ctx.bezierCurveTo(Infinity, 50, Infinity, Infinity, 0, Infinity);
           ctx.bezierCurveTo(Infinity, 50, Infinity, 50, Infinity, 50);
           ctx.bezierCurveTo(Infinity, 50, Infinity, 50, Infinity, Infinity);
           ctx.bezierCurveTo(Infinity, 50, Infinity, 50, 0, Infinity);
           ctx.bezierCurveTo(Infinity, 50, 0, Infinity, 0, 50);
           ctx.bezierCurveTo(Infinity, 50, 0, Infinity, Infinity, 50);
           ctx.bezierCurveTo(Infinity, 50, 0, Infinity, Infinity, Infinity);
           ctx.bezierCurveTo(Infinity, 50, 0, Infinity, 0, Infinity);
           ctx.bezierCurveTo(Infinity, 50, 0, 50, Infinity, 50);
           ctx.bezierCurveTo(Infinity, 50, 0, 50, Infinity, Infinity);
           ctx.bezierCurveTo(Infinity, 50, 0, 50, 0, Infinity);
           ctx.bezierCurveTo(0, Infinity, Infinity, 50, 0, 50);
           ctx.bezierCurveTo(0, Infinity, Infinity, Infinity, 0, 50);
           ctx.bezierCurveTo(0, Infinity, Infinity, Infinity, Infinity, 50);
           ctx.bezierCurveTo(0, Infinity, Infinity, Infinity, Infinity, Infinity);
           ctx.bezierCurveTo(0, Infinity, Infinity, Infinity, 0, Infinity);
           ctx.bezierCurveTo(0, Infinity, Infinity, 50, Infinity, 50);
           ctx.bezierCurveTo(0, Infinity, Infinity, 50, Infinity, Infinity);
           ctx.bezierCurveTo(0, Infinity, Infinity, 50, 0, Infinity);
           ctx.bezierCurveTo(0, Infinity, 0, Infinity, 0, 50);
           ctx.bezierCurveTo(0, Infinity, 0, Infinity, Infinity, 50);
           ctx.bezierCurveTo(0, Infinity, 0, Infinity, Infinity, Infinity);
           ctx.bezierCurveTo(0, Infinity, 0, Infinity, 0, Infinity);
           ctx.bezierCurveTo(0, Infinity, 0, 50, Infinity, 50);
           ctx.bezierCurveTo(0, Infinity, 0, 50, Infinity, Infinity);
           ctx.bezierCurveTo(0, Infinity, 0, 50, 0, Infinity);
           ctx.bezierCurveTo(0, 50, Infinity, Infinity, 0, 50);
           ctx.bezierCurveTo(0, 50, Infinity, Infinity, Infinity, 50);
           ctx.bezierCurveTo(0, 50, Infinity, Infinity, Infinity, Infinity);
           ctx.bezierCurveTo(0, 50, Infinity, Infinity, 0, Infinity);
           ctx.bezierCurveTo(0, 50, Infinity, 50, Infinity, 50);
           ctx.bezierCurveTo(0, 50, Infinity, 50, Infinity, Infinity);
           ctx.bezierCurveTo(0, 50, Infinity, 50, 0, Infinity);
           ctx.bezierCurveTo(0, 50, 0, Infinity, Infinity, 50);
           ctx.bezierCurveTo(0, 50, 0, Infinity, Infinity, Infinity);
           ctx.bezierCurveTo(0, 50, 0, Infinity, 0, Infinity);
           ctx.bezierCurveTo(0, 50, 0, 50, Infinity, Infinity);
           ctx.lineTo(100, 50);
           ctx.lineTo(0, 50);
           ctx.fillStyle = '#0f0';
           ctx.fill();
           comparePixel(ctx, 50,25, 0,255,0,255);
           comparePixel(ctx, 90,45, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.scale(1000, 1000);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 0.055;
           ctx.beginPath();
           ctx.moveTo(-2, 3.1);
           ctx.bezierCurveTo(-2, -1, 2.1, -1, 2.1, 3.1);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);
           comparePixel(ctx, 1,1, 0,255,0,255);
           comparePixel(ctx, 98,1, 0,255,0,255);
           comparePixel(ctx, 1,48, 0,255,0,255);
           comparePixel(ctx, 98,48, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 55;
           ctx.beginPath();
           ctx.moveTo(-2000, 3100);
           ctx.bezierCurveTo(-2000, -1000, 2100, -1000, 2100, 3100);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);
           comparePixel(ctx, 1,1, 0,255,0,255);
           comparePixel(ctx, 98,1, 0,255,0,255);
           comparePixel(ctx, 1,48, 0,255,0,255);
           comparePixel(ctx, 98,48, 0,255,0,255);
           canvas.destroy()
       }
       function test_quadraticCurveTo(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 50;
           ctx.beginPath();
           ctx.moveTo(0, 25);
           ctx.quadraticCurveTo(100, 25, 100, 25);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#f00';
           ctx.lineWidth = 50;
           ctx.beginPath();
           ctx.quadraticCurveTo(100, 50, 200, 50);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);
           comparePixel(ctx, 95,45, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 50;
           ctx.beginPath();
           ctx.quadraticCurveTo(0, 25, 100, 25);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);
           comparePixel(ctx, 5,45, 0,255,0,255);

           ctx.reset();
           ctx.moveTo(0, 0);
           ctx.lineTo(100, 0);
           ctx.quadraticCurveTo(Infinity, 50, 0, 50);
           ctx.quadraticCurveTo(-Infinity, 50, 0, 50);
           ctx.quadraticCurveTo(NaN, 50, 0, 50);
           ctx.quadraticCurveTo(0, Infinity, 0, 50);
           ctx.quadraticCurveTo(0, -Infinity, 0, 50);
           ctx.quadraticCurveTo(0, NaN, 0, 50);
           ctx.quadraticCurveTo(0, 50, Infinity, 50);
           ctx.quadraticCurveTo(0, 50, -Infinity, 50);
           ctx.quadraticCurveTo(0, 50, NaN, 50);
           ctx.quadraticCurveTo(0, 50, 0, Infinity);
           ctx.quadraticCurveTo(0, 50, 0, -Infinity);
           ctx.quadraticCurveTo(0, 50, 0, NaN);
           ctx.quadraticCurveTo(Infinity, Infinity, 0, 50);
           ctx.quadraticCurveTo(Infinity, Infinity, Infinity, 50);
           ctx.quadraticCurveTo(Infinity, Infinity, Infinity, Infinity);
           ctx.quadraticCurveTo(Infinity, Infinity, 0, Infinity);
           ctx.quadraticCurveTo(Infinity, 50, Infinity, 50);
           ctx.quadraticCurveTo(Infinity, 50, Infinity, Infinity);
           ctx.quadraticCurveTo(Infinity, 50, 0, Infinity);
           ctx.quadraticCurveTo(0, Infinity, Infinity, 50);
           ctx.quadraticCurveTo(0, Infinity, Infinity, Infinity);
           ctx.quadraticCurveTo(0, Infinity, 0, Infinity);
           ctx.quadraticCurveTo(0, 50, Infinity, Infinity);
           ctx.lineTo(100, 50);
           ctx.lineTo(0, 50);
           ctx.fillStyle = '#0f0';
           ctx.fill();
           comparePixel(ctx, 50,25, 0,255,0,255);
           comparePixel(ctx, 90,45, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.scale(1000, 1000);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 0.055;
           ctx.beginPath();
           ctx.moveTo(-1, 1.05);
           ctx.quadraticCurveTo(0, -1, 1.2, 1.05);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);
           //comparePixel(ctx, 1,1, 0,255,0,255);
           //comparePixel(ctx, 98,1, 0,255,0,255);
           comparePixel(ctx, 1,48, 0,255,0,255);
           comparePixel(ctx, 98,48, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 55;
           ctx.beginPath();
           ctx.moveTo(-1000, 1050);
           ctx.quadraticCurveTo(0, -1000, 1200, 1050);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);
           //comparePixel(ctx, 1,1, 0,255,0,255);
           //comparePixel(ctx, 98,1, 0,255,0,255);
           comparePixel(ctx, 1,48, 0,255,0,255);
           comparePixel(ctx, 98,48, 0,255,0,255);
           canvas.destroy()
       }
       function test_rect(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.fillStyle = '#0f0';
           ctx.rect(0, 0, 100, 50);
           ctx.fill();
           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 200;
           ctx.lineJoin = 'miter';
           ctx.rect(100, 50, 100, 100);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 100;
           ctx.rect(200, 100, 400, 1000);
           ctx.lineTo(-2000, -1000);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 450;
           ctx.lineCap = 'round';
           ctx.lineJoin = 'bevel';
           ctx.rect(150, 150, 2000, 2000);
           ctx.lineTo(160, 160);
           ctx.stroke();
           comparePixel(ctx, 1,1, 0,255,0,255);
           comparePixel(ctx, 98,1, 0,255,0,255);
           comparePixel(ctx, 1,48, 0,255,0,255);
           comparePixel(ctx, 98,48, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.beginPath();
           ctx.fillStyle = '#0f0';
           ctx.rect(0, 0, 50, 25);
           ctx.rect(100, 0, -50, 25);
           ctx.rect(0, 50, 50, -25);
           ctx.rect(100, 50, -50, -25);
           ctx.fill();
           comparePixel(ctx, 25,12, 0,255,0,255);
           comparePixel(ctx, 75,12, 0,255,0,255);
           comparePixel(ctx, 25,37, 0,255,0,255);
           comparePixel(ctx, 75,37, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.beginPath();
           ctx.strokeStyle = '#f00';
           ctx.lineWidth = 50;
           ctx.moveTo(-100, 25);
           ctx.lineTo(-50, 25);
           ctx.rect(200, 25, 1, 1);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);


           ctx.reset();
           ctx.moveTo(0, 0);
           ctx.lineTo(100, 0);
           ctx.rect(Infinity, 50, 1, 1);
           ctx.rect(-Infinity, 50, 1, 1);
           ctx.rect(NaN, 50, 1, 1);
           ctx.rect(0, Infinity, 1, 1);
           ctx.rect(0, -Infinity, 1, 1);
           ctx.rect(0, NaN, 1, 1);
           ctx.rect(0, 50, Infinity, 1);
           ctx.rect(0, 50, -Infinity, 1);
           ctx.rect(0, 50, NaN, 1);
           ctx.rect(0, 50, 1, Infinity);
           ctx.rect(0, 50, 1, -Infinity);
           ctx.rect(0, 50, 1, NaN);
           ctx.rect(Infinity, Infinity, 1, 1);
           ctx.rect(Infinity, Infinity, Infinity, 1);
           ctx.rect(Infinity, Infinity, Infinity, Infinity);
           ctx.rect(Infinity, Infinity, 1, Infinity);
           ctx.rect(Infinity, 50, Infinity, 1);
           ctx.rect(Infinity, 50, Infinity, Infinity);
           ctx.rect(Infinity, 50, 1, Infinity);
           ctx.rect(0, Infinity, Infinity, 1);
           ctx.rect(0, Infinity, Infinity, Infinity);
           ctx.rect(0, Infinity, 1, Infinity);
           ctx.rect(0, 50, Infinity, Infinity);
           ctx.lineTo(100, 50);
           ctx.lineTo(0, 50);
           ctx.fillStyle = '#0f0';
           ctx.fill();
           comparePixel(ctx, 50,25, 0,255,0,255);
           comparePixel(ctx, 90,45, 0,255,0,255);


           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 90;
           ctx.beginPath();
           ctx.rect(45, 20, 10, 10);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.beginPath();
           ctx.fillStyle = '#f00';
           ctx.rect(0, 0, 50, 50);
           ctx.rect(100, 50, -50, -50);
           ctx.rect(0, 25, 100, -25);
           ctx.rect(100, 25, -100, 25);
           ctx.fill();
           comparePixel(ctx, 25,12, 0,255,0,255);
           comparePixel(ctx, 75,12, 0,255,0,255);
           comparePixel(ctx, 25,37, 0,255,0,255);
           comparePixel(ctx, 75,37, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 100;
           ctx.beginPath();
           ctx.rect(0, 50, 100, 0);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 100;
           ctx.beginPath();
           ctx.rect(50, -100, 0, 250);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#f00';
           ctx.lineWidth = 100;
           ctx.beginPath();
           ctx.rect(50, 25, 0, 0);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 50;
           ctx.rect(100, 25, 0, 0);
           ctx.lineTo(0, 25);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#f00';
           ctx.lineWidth = 50;
           ctx.moveTo(0, 0);
           ctx.rect(100, 25, 0, 0);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#f00';
           ctx.lineJoin = 'miter';
           ctx.miterLimit = 1.5;
           ctx.lineWidth = 200;
           ctx.beginPath();
           ctx.rect(100, 25, 1000, 0);
           ctx.stroke();
           //comparePixel(ctx, 50,25, 0,255,0,255);
           canvas.destroy()
       }

       function test_clearRect(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.beginPath();
           ctx.rect(0, 0, 100, 50);
           ctx.clearRect(0, 0, 16, 16);
           ctx.fill();
           comparePixel(ctx, 50,25, 0,255,0,255);
           canvas.destroy()
       }
       function test_fillRect(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();
           ctx.beginPath();
           ctx.rect(0, 0, 100, 50);
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 16, 16);
           ctx.fillStyle = '#0f0';
           ctx.fill();
           comparePixel(ctx, 50,25, 0,255,0,255);
           canvas.destroy()
       }

       function test_strokeRect(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();
           ctx.beginPath();
           ctx.rect(0, 0, 100, 50);
           ctx.strokeStyle = '#f00';
           ctx.lineWidth = 5;
           ctx.strokeRect(0, 0, 16, 16);
           ctx.fillStyle = '#0f0';
           ctx.fill();

           comparePixel(ctx, 50,25, 0,255,0,255);
           canvas.destroy()
       }
       function test_transform(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);

           ctx.translate(-100, 0);
           ctx.rect(100, 0, 100, 50);
           ctx.translate(0, -100);
           ctx.fillStyle = '#0f0';
           ctx.fill();

           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.fillStyle = '#0f0';
           ctx.moveTo(0, 0);
           ctx.translate(100, 0);
           ctx.lineTo(0, 0);
           ctx.translate(0, 50);
           ctx.lineTo(0, 0);
           ctx.translate(-100, 0);
           ctx.lineTo(0, 0);
           ctx.translate(1000, 1000);
           ctx.rotate(Math.PI/2);
           ctx.scale(0.1, 0.1);
           ctx.fill();
           comparePixel(ctx, 50,25, 0,255,0,255);

           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);

           ctx.fillStyle = '#f00';
           ctx.translate(-100, 0);
           ctx.rect(0, 0, 100, 50);
           ctx.fill();
           ctx.translate(100, 0);
           ctx.fill();

           ctx.beginPath();
           ctx.strokeStyle = '#f00';
           ctx.lineWidth = 50;
           ctx.translate(0, -50);
           ctx.moveTo(0, 25);
           ctx.lineTo(100, 25);
           ctx.stroke();
           ctx.translate(0, 50);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);
           canvas.destroy()
       }
}
