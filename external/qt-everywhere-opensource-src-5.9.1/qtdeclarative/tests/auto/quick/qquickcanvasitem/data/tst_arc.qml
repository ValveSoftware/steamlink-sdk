import QtQuick 2.0

CanvasTestCase {
   id:testCase
   name: "arc"
   function init_data() { return testData("2d"); }
       function test_angle_1(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.fillStyle = '#f00';
           ctx.beginPath();
           ctx.moveTo(100, 0);
           ctx.arc(100, 0, 150, Math.PI/2, -Math.PI, true);
           ctx.fill();
           comparePixel(ctx,50,25, 0,255,0,255);
           canvas.destroy();
        }
       function test_angle_2(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.fillStyle = '#f00';
           ctx.beginPath();
           ctx.moveTo(100, 0);
           ctx.arc(100, 0, 150, -3*Math.PI/2, -Math.PI, true);
           ctx.fill();
           comparePixel(ctx,50,25, 0,255,0,255);
           canvas.destroy();
        }
       function test_angle_3(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();
           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.fillStyle = '#f00';
           ctx.beginPath();
           ctx.moveTo(100, 0);
           ctx.arc(100, 0, 150, (512+1/2)*Math.PI, (1024-1)*Math.PI, true);
           ctx.fill();
           /*FIXME: from: http://www.w3.org/TR/2dcontext/#dom-context-2d-arc
           If the anticlockwise argument is omitted or false and endAngle-startAngle is equal to or greater than 2π, or, if the anticlockwise argument is true and startAngle-endAngle is equal to or greater than 2π, then the arc is the whole circumference of this circle.
           //comparePixel(ctx,50,25, 0,255,0,255);
           */
           canvas.destroy();
        }
       function test_angle_4(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.fillStyle = '#0f0';
           ctx.beginPath();
           ctx.moveTo(50, 25);
           ctx.arc(50, 25, 60, (512+1/2)*Math.PI, (1024-1)*Math.PI, false);
           ctx.fill();
           comparePixel(ctx,1,1, 0,255,0,255);
           comparePixel(ctx,98,1, 0,255,0,255);
           comparePixel(ctx,1,48, 0,255,0,255);
           comparePixel(ctx,98,48, 0,255,0,255);
           canvas.destroy();
        }
       function test_angle_5(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.fillStyle = '#f00';
           ctx.beginPath();
           ctx.moveTo(100, 0);
           ctx.arc(100, 0, 150, (1024-1)*Math.PI, (512+1/2)*Math.PI, false);
           ctx.fill();
           /*FIXME: from: http://www.w3.org/TR/2dcontext/#dom-context-2d-arc
           If the anticlockwise argument is omitted or false and endAngle-startAngle is equal to or greater than 2π, or, if the anticlockwise argument is true and startAngle-endAngle is equal to or greater than 2π, then the arc is the whole circumference of this circle.
           //comparePixel(ctx,50,25, 0,255,0,255);
           */
           canvas.destroy();
        }

       function test_angle_6(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.fillStyle = '#0f0';
           ctx.beginPath();
           ctx.moveTo(50, 25);
           ctx.arc(50, 25, 60, (1024-1)*Math.PI, (512+1/2)*Math.PI, true);
           ctx.fill();

           comparePixel(ctx,1,1, 0,255,0,255);
           comparePixel(ctx,98,1, 0,255,0,255);
           comparePixel(ctx,1,48, 0,255,0,255);
           comparePixel(ctx,98,48, 0,255,0,255);
           canvas.destroy();
        }

       function test_empty(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.lineWidth = 50;
           ctx.strokeStyle = '#f00';
           ctx.beginPath();
           ctx.arc(200, 25, 5, 0, 2*Math.PI, true);
           ctx.stroke();
           comparePixel(ctx,50,25, 0,255,0,255);
           canvas.destroy();
        }
       function test_nonempty(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.lineWidth = 50;
           ctx.strokeStyle = '#0f0';
           ctx.beginPath();
           ctx.moveTo(0, 25);
           ctx.arc(200, 25, 5, 0, 2*Math.PI, true);
           ctx.stroke();
           comparePixel(ctx,50,25, 0,255,0,255);
           canvas.destroy();
        }
       function test_nonfinite(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.moveTo(0, 0);
           ctx.lineTo(100, 0);
           ctx.arc(Infinity, 0, 50, 0, 2*Math.PI, true);
           ctx.arc(-Infinity, 0, 50, 0, 2*Math.PI, true);
           ctx.arc(NaN, 0, 50, 0, 2*Math.PI, true);
           ctx.arc(0, Infinity, 50, 0, 2*Math.PI, true);
           ctx.arc(0, -Infinity, 50, 0, 2*Math.PI, true);
           ctx.arc(0, NaN, 50, 0, 2*Math.PI, true);
           ctx.arc(0, 0, Infinity, 0, 2*Math.PI, true);
           ctx.arc(0, 0, -Infinity, 0, 2*Math.PI, true);
           ctx.arc(0, 0, NaN, 0, 2*Math.PI, true);
           ctx.arc(0, 0, 50, Infinity, 2*Math.PI, true);
           ctx.arc(0, 0, 50, -Infinity, 2*Math.PI, true);
           ctx.arc(0, 0, 50, NaN, 2*Math.PI, true);
           ctx.arc(0, 0, 50, 0, Infinity, true);
           ctx.arc(0, 0, 50, 0, -Infinity, true);
           ctx.arc(0, 0, 50, 0, NaN, true);
           ctx.arc(Infinity, Infinity, 50, 0, 2*Math.PI, true);
           ctx.arc(Infinity, Infinity, Infinity, 0, 2*Math.PI, true);
           ctx.arc(Infinity, Infinity, Infinity, Infinity, 2*Math.PI, true);
           ctx.arc(Infinity, Infinity, Infinity, Infinity, Infinity, true);
           ctx.arc(Infinity, Infinity, Infinity, 0, Infinity, true);
           ctx.arc(Infinity, Infinity, 50, Infinity, 2*Math.PI, true);
           ctx.arc(Infinity, Infinity, 50, Infinity, Infinity, true);
           ctx.arc(Infinity, Infinity, 50, 0, Infinity, true);
           ctx.arc(Infinity, 0, Infinity, 0, 2*Math.PI, true);
           ctx.arc(Infinity, 0, Infinity, Infinity, 2*Math.PI, true);
           ctx.arc(Infinity, 0, Infinity, Infinity, Infinity, true);
           ctx.arc(Infinity, 0, Infinity, 0, Infinity, true);
           ctx.arc(Infinity, 0, 50, Infinity, 2*Math.PI, true);
           ctx.arc(Infinity, 0, 50, Infinity, Infinity, true);
           ctx.arc(Infinity, 0, 50, 0, Infinity, true);
           ctx.arc(0, Infinity, Infinity, 0, 2*Math.PI, true);
           ctx.arc(0, Infinity, Infinity, Infinity, 2*Math.PI, true);
           ctx.arc(0, Infinity, Infinity, Infinity, Infinity, true);
           ctx.arc(0, Infinity, Infinity, 0, Infinity, true);
           ctx.arc(0, Infinity, 50, Infinity, 2*Math.PI, true);
           ctx.arc(0, Infinity, 50, Infinity, Infinity, true);
           ctx.arc(0, Infinity, 50, 0, Infinity, true);
           ctx.arc(0, 0, Infinity, Infinity, 2*Math.PI, true);
           ctx.arc(0, 0, Infinity, Infinity, Infinity, true);
           ctx.arc(0, 0, Infinity, 0, Infinity, true);
           ctx.arc(0, 0, 50, Infinity, Infinity, true);
           ctx.lineTo(100, 50);
           ctx.lineTo(0, 50);
           ctx.fillStyle = '#0f0';
           ctx.fill();
           comparePixel(ctx,50,25, 0,255,0,255);
           comparePixel(ctx,90,45, 0,255,0,255);
           canvas.destroy();
       }
       function test_end(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.lineWidth = 50;
           ctx.strokeStyle = '#0f0';
           ctx.beginPath();
           ctx.moveTo(-100, 0);
           ctx.arc(-100, 0, 25, -Math.PI/2, Math.PI/2, true);
           ctx.lineTo(100, 25);
           ctx.stroke();
           comparePixel(ctx,50,25, 0,255,0,255);
           canvas.destroy();
        }
       function test_negative(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           try { var err = false;
             ctx.arc(0, 0, -1, 0, 0, true);
           } catch (e) {
               if (e.code != DOMException.INDEX_SIZE_ERR)
                   fail("expected exception of type INDEX_SIZE_ERR, got: "+e.message);
               err = true;
           } finally {
               verify(err, "should throw exception of type INDEX_SIZE_ERR: ctx.arc(0, 0, -1, 0, 0, true)");
           }

           canvas.destroy();
       }

       function test_scale_1(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.scale(2, 0.5);
           ctx.fillStyle = '#0f0';
           ctx.beginPath();
           ctx.arc(25, 50, 56, 0, 2*Math.PI, false);
           ctx.fill();
           ctx.fillStyle = '#f00';
           ctx.beginPath();
           ctx.moveTo(-25, 50);
           ctx.arc(-25, 50, 24, 0, 2*Math.PI, false);
           ctx.moveTo(75, 50);
           ctx.arc(75, 50, 24, 0, 2*Math.PI, false);
           ctx.moveTo(25, -25);
           ctx.arc(25, -25, 24, 0, 2*Math.PI, false);
           ctx.moveTo(25, 125);
           ctx.arc(25, 125, 24, 0, 2*Math.PI, false);
           ctx.fill();

           comparePixel(ctx, 0,0, 0,255,0,255);
           comparePixel(ctx, 50,0, 0,255,0,255);
           comparePixel(ctx, 99,0, 0,255,0,255);
           comparePixel(ctx, 0,25, 0,255,0,255);
           comparePixel(ctx, 50,25, 0,255,0,255);
           comparePixel(ctx, 99,25, 0,255,0,255);
           comparePixel(ctx, 0,49, 0,255,0,255);
           comparePixel(ctx, 50,49, 0,255,0,255);
           comparePixel(ctx, 99,49, 0,255,0,255);
           canvas.destroy();
       }

       function test_scale_2(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.scale(100, 100);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 1.2;
           ctx.beginPath();
           ctx.arc(0, 0, 0.6, 0, Math.PI/2, false);
           ctx.stroke();

           comparePixel(ctx, 1,1, 0,255,0,255);
           comparePixel(ctx, 50,1, 0,255,0,255);
           comparePixel(ctx, 98,1, 0,255,0,255);
           comparePixel(ctx, 1,25, 0,255,0,255);
           comparePixel(ctx, 50,25, 0,255,0,255);
           comparePixel(ctx, 98,25, 0,255,0,255);
           comparePixel(ctx, 1,48, 0,255,0,255);
           comparePixel(ctx, 50,48, 0,255,0,255);
           comparePixel(ctx, 98,48, 0,255,0,255);
           canvas.destroy();
       }

       function test_selfintersect_1(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.lineWidth = 200;
           ctx.strokeStyle = '#f00';
           ctx.beginPath();
           ctx.arc(100, 50, 25, 0, -Math.PI/2, true);
           ctx.stroke();
           ctx.beginPath();
           ctx.arc(0, 0, 25, 0, -Math.PI/2, true);
           ctx.stroke();
           comparePixel(ctx, 1,1, 0,255,0,255);
           comparePixel(ctx, 50,25, 0,255,0,255);
           canvas.destroy();
       }

       function test_selfintersect_2(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.lineWidth = 180;
           ctx.strokeStyle = '#0f0';
           ctx.beginPath();
           ctx.arc(-50, 50, 25, 0, -Math.PI/2, true);
           ctx.stroke();
           ctx.beginPath();
           ctx.arc(100, 0, 25, 0, -Math.PI/2, true);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);
           comparePixel(ctx, 90,10, 0,255,0,255);
           comparePixel(ctx, 97,1, 0,255,0,255);
           comparePixel(ctx, 97,2, 0,255,0,255);
           comparePixel(ctx, 97,3, 0,255,0,255);
           comparePixel(ctx, 2,48, 0,255,0,255);
           canvas.destroy();
       }

       function test_shape_1(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.lineWidth = 50;
           ctx.strokeStyle = '#f00';
           ctx.beginPath();
           ctx.arc(50, 50, 50, 0, Math.PI, false);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);
           comparePixel(ctx, 1,1, 0,255,0,255);
           comparePixel(ctx, 98,1, 0,255,0,255);
           comparePixel(ctx, 1,48, 0,255,0,255);
           comparePixel(ctx, 20,48, 0,255,0,255);
           comparePixel(ctx, 98,48, 0,255,0,255);
           canvas.destroy();
       }

       function test_shape_2(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.lineWidth = 100;
           ctx.strokeStyle = '#0f0';
           ctx.beginPath();
           ctx.arc(50, 50, 50, 0, Math.PI, true);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);
           comparePixel(ctx, 1,1, 0,255,0,255);
           comparePixel(ctx, 98,1, 0,255,0,255);
           comparePixel(ctx, 1,48, 0,255,0,255);
           comparePixel(ctx, 20,48, 0,255,0,255);
           comparePixel(ctx, 98,48, 0,255,0,255);
           canvas.destroy();
       }
       function test_shape_3(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.lineWidth = 100;
           ctx.strokeStyle = '#f00';
           ctx.beginPath();
           ctx.arc(0, 50, 50, 0, -Math.PI/2, false);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);
           comparePixel(ctx, 1,1, 0,255,0,255);
           comparePixel(ctx, 98,1, 0,255,0,255);
           comparePixel(ctx, 1,48, 0,255,0,255);
           comparePixel(ctx, 98,48, 0,255,0,255);
           canvas.destroy();
       }

       function test_shape_4(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.lineWidth = 150;
           ctx.strokeStyle = '#0f0';
           ctx.beginPath();
           ctx.arc(-50, 50, 100, 0, -Math.PI/2, true);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);
           comparePixel(ctx, 1,1, 0,255,0,255);
           comparePixel(ctx, 98,1, 0,255,0,255);
           comparePixel(ctx, 1,48, 0,255,0,255);
           comparePixel(ctx, 98,48, 0,255,0,255);
           canvas.destroy();
       }

       function test_shape_5(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.lineWidth = 200;
           ctx.strokeStyle = '#f00';
           ctx.beginPath();
           ctx.arc(300, 0, 100, 0, 5*Math.PI, false);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);
           comparePixel(ctx, 1,1, 0,255,0,255);
           comparePixel(ctx, 98,1, 0,255,0,255);
           comparePixel(ctx, 1,48, 0,255,0,255);
           comparePixel(ctx, 98,48, 0,255,0,255);
           canvas.destroy();
       }

       function test_twopie(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#f00';
           ctx.lineWidth = 100;
           ctx.beginPath();
           ctx.arc(50, 25, 50, 0, 2*Math.PI - 1e-4, true);
           ctx.stroke();
           comparePixel(ctx, 50,20, 0,255,0,255);
           ctx.reset();

           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 100;
           ctx.beginPath();
           ctx.arc(50, 25, 50, 0, 2*Math.PI - 1e-4, false);
           ctx.stroke();
           comparePixel(ctx, 50,20, 0,255,0,255);
           ctx.reset();

           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 100;
           ctx.beginPath();
           ctx.arc(50, 25, 50, 0, 2*Math.PI + 1e-4, true);
           ctx.stroke();
           //FIXME:still different behavior from browsers, > 2pi span issue
           //comparePixel(ctx, 50,20, 0,255,0,255);
           ctx.reset();

           ctx.fillStyle = '#f00';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#0f0';
           ctx.lineWidth = 100;
           ctx.beginPath();
           ctx.arc(50, 25, 50, 0, 2*Math.PI + 1e-4, false);
           ctx.stroke();
           comparePixel(ctx, 50,20, 0,255,0,255);
           canvas.destroy();
       }

       function test_zero(row) {
           var canvas = createCanvasObject(row);
           var ctx = canvas.getContext('2d');
           ctx.reset();

           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#f00';
           ctx.lineWidth = 100;
           ctx.beginPath();
           ctx.arc(50, 25, 50, 0, 0, true);
           ctx.stroke();
           comparePixel(ctx, 50,20, 0,255,0,255);
           ctx.reset();

           ctx.fillStyle = '#0f0';
           ctx.fillRect(0, 0, 100, 50);
           ctx.strokeStyle = '#f00';
           ctx.lineWidth = 100;
           ctx.beginPath();
           ctx.arc(50, 25, 50, 0, 0, false);
           ctx.stroke();
           comparePixel(ctx, 50,20, 0,255,0,255);
           ctx.reset();

           ctx.fillStyle = '#f00'
           ctx.fillRect(0, 0, 100, 50);
           ctx.lineWidth = 50;
           ctx.strokeStyle = '#0f0';
           ctx.beginPath();
           ctx.moveTo(0, 25);
           ctx.arc(200, 25, 0, 0, Math.PI, true);
           ctx.stroke();
           comparePixel(ctx, 50,25, 0,255,0,255);
           canvas.destroy();
       }
}