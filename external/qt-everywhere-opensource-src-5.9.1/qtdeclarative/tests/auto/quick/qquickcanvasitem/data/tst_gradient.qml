import QtQuick 2.0

CanvasTestCase {
   id:testCase
   name: "gradient"
   function init_data() { return testData("2d"); }
   function test_basic(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);
       var g = ctx.createLinearGradient(0, 0, 0, 50);
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);
       //comparePixel(ctx, 50,25, 0,255,0,255,2);
   }

   function test_interpolate(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();

       ctx.fillStyle = '#ff0';
       ctx.fillRect(0, 0, 100, 50);
       var g = ctx.createLinearGradient(0, 0, 100, 0);
       g.addColorStop(0, 'rgba(0,0,255, 0)');
       g.addColorStop(1, 'rgba(0,0,255, 1)');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);
       //comparePixel(ctx, 25,25, 191,191,63,255,3);
       //comparePixel(ctx, 50,25, 127,127,127,255,3);
       //comparePixel(ctx, 75,25, 63,63,191,255,3);

       ctx.reset();
       var g = ctx.createLinearGradient(0, 0, 100, 0);
       g.addColorStop(0, '#ff0');
       g.addColorStop(1, '#00f');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);
       //comparePixel(ctx, 25,25, 191,191,63,255,3);
       //comparePixel(ctx, 50,25, 127,127,127,255,3);
       //comparePixel(ctx, 75,25, 63,63,191,255,3);


       ctx.reset();
       var g = ctx.createLinearGradient(0, 0, 100, 0);
       g.addColorStop(0, 'rgba(255,255,0, 0)');
       g.addColorStop(1, 'rgba(0,0,255, 1)');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);
       //comparePixel(ctx, 25,25, 191,191,63,63,3);
       //comparePixel(ctx, 50,25, 127,127,127,127,3);
       //comparePixel(ctx, 75,25, 63,63,191,191,3);

       ctx.reset();
       canvas.width = 200;
       var g = ctx.createLinearGradient(0, 0, 200, 0);
       g.addColorStop(0, '#ff0');
       g.addColorStop(0.5, '#0ff');
       g.addColorStop(1, '#f0f');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 200, 50);
       //comparePixel(ctx, 50,25, 127,255,127,255,3);
       //comparePixel(ctx, 100,25, 0,255,255,255,3);
       //comparePixel(ctx, 150,25, 127,127,255,255,3);

       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       var g = ctx.createLinearGradient(25, 0, 75, 0);
       g.addColorStop(0.4, '#0f0');
       g.addColorStop(0.6, '#0f0');

       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);
       //comparePixel(ctx, 20,25, 0,255,0,255,2);
       //comparePixel(ctx, 50,25, 0,255,0,255,2);
       //comparePixel(ctx, 80,25, 0,255,0,255,2);


       ctx.reset();
       ctx.canvas.width = 200;
       var g = ctx.createLinearGradient(0, 0, 200, 0);
       g.addColorStop(0, '#f00');
       g.addColorStop(0, '#ff0');
       g.addColorStop(0.25, '#00f');
       g.addColorStop(0.25, '#0f0');
       g.addColorStop(0.25, '#0f0');
       g.addColorStop(0.25, '#0f0');
       g.addColorStop(0.25, '#ff0');
       g.addColorStop(0.5, '#00f');
       g.addColorStop(0.5, '#0f0');
       g.addColorStop(0.75, '#00f');
       g.addColorStop(0.75, '#f00');
       g.addColorStop(0.75, '#ff0');
       g.addColorStop(0.5, '#0f0');
       g.addColorStop(0.5, '#0f0');
       g.addColorStop(0.5, '#ff0');
       g.addColorStop(1, '#00f');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 200, 50);
       //comparePixel(ctx, 49,25, 0,0,255,255,16);
       //comparePixel(ctx, 51,25, 255,255,0,255,16);
       //comparePixel(ctx, 99,25, 0,0,255,255,16);
       //comparePixel(ctx, 101,25, 255,255,0,255,16);
       //comparePixel(ctx, 149,25, 0,0,255,255,16);
       //comparePixel(ctx, 151,25, 255,255,0,255,16);
       ctx.canvas.width = 100;

       ctx.reset();
       var g = ctx.createLinearGradient(0, 0, 100, 0);
       var ps = [ 0, 1/10, 1/4, 1/3, 1/2, 3/4, 1 ];
       for (var p = 0; p < ps.length; ++p)
       {
               g.addColorStop(ps[p], '#0f0');
               for (var i = 0; i < 15; ++i)
                       g.addColorStop(ps[p], '#f00');
               g.addColorStop(ps[p], '#0f0');
       }
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);
       //comparePixel(ctx, 1,25, 0,255,0,255);
       //comparePixel(ctx, 30,25, 0,255,0,255);
       //comparePixel(ctx, 40,25, 0,255,0,255);
       //comparePixel(ctx, 60,25, 0,255,0,255);
       //comparePixel(ctx, 80,25, 0,255,0,255);


       ctx.reset();
       var g = ctx.createLinearGradient(0, 0, 100, 0);
       g.addColorStop(0, '#0f0');
       g.addColorStop(1, '#0f0');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);
       //comparePixel(ctx, 50,25, 0,255,0,255);



       ctx.reset();
       var g = ctx.createLinearGradient(0, 0, 0, 50);
       g.addColorStop(0, '#ff0');
       g.addColorStop(1, '#00f');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);
       //comparePixel(ctx, 50,12, 191,191,63,255,10);
       //comparePixel(ctx, 50,25, 127,127,127,255,5);
       //comparePixel(ctx, 50,37, 63,63,191,255,10);


       ctx.reset();

       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       var g = ctx.createLinearGradient(50, 25, 50, 25); // zero-length line (undefined direction)
       g.addColorStop(0, '#f00');
       g.addColorStop(1, '#f00');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);
       //comparePixel(ctx, 40,20, 0,255,0,255,2);



   }
   function test_radial(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       var g = ctx.createRadialGradient(0, 100, 40, 100, 100, 50);
       g.addColorStop(0, '#f00');
       g.addColorStop(1, '#f00');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 50,1, 0,255,0,255);
       //comparePixel(ctx, 98,1, 0,255,0,255);
       //comparePixel(ctx, 1,25, 0,255,0,255,16);
       //comparePixel(ctx, 50,25, 0,255,0,255);
       //comparePixel(ctx, 98,25, 0,255,0,255);
       //comparePixel(ctx, 1,48, 0,255,0,255);
       //comparePixel(ctx, 50,48, 0,255,0,255);
       //comparePixel(ctx, 98,48, 0,255,0,255);

       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       g = ctx.createRadialGradient(210, 25, 100, 230, 25, 101);
       g.addColorStop(0, '#0f0');
       g.addColorStop(1, '#f00');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 50,1, 0,255,0,255);
       //comparePixel(ctx, 98,1, 0,255,0,255);
       //comparePixel(ctx, 1,25, 0,255,0,255,16);
       //comparePixel(ctx, 50,25, 0,255,0,255);
       //comparePixel(ctx, 98,25, 0,255,0,255);
       //comparePixel(ctx, 1,48, 0,255,0,255);
       //comparePixel(ctx, 50,48, 0,255,0,255);
       //comparePixel(ctx, 98,48, 0,255,0,255);


       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       g = ctx.createRadialGradient(210, 25, 100, 230, 25, 100);
       g.addColorStop(0, '#0f0');
       g.addColorStop(1, '#f00');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 50,1, 0,255,0,255);
       //comparePixel(ctx, 98,1, 0,255,0,255);
       //comparePixel(ctx, 1,25, 0,255,0,255,16);
       //comparePixel(ctx, 50,25, 0,255,0,255);
       //comparePixel(ctx, 98,25, 0,255,0,255);
       //comparePixel(ctx, 1,48, 0,255,0,255);
       //comparePixel(ctx, 50,48, 0,255,0,255);
       //comparePixel(ctx, 98,48, 0,255,0,255);

       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       g = ctx.createRadialGradient(311, 25, 10, 210, 25, 100);
       g.addColorStop(0, '#f00');
       g.addColorStop(1, '#0f0');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 50,1, 0,255,0,255);
       //comparePixel(ctx, 98,1, 0,255,0,255);
       //comparePixel(ctx, 1,25, 0,255,0,255,16);
       //comparePixel(ctx, 50,25, 0,255,0,255);
       //comparePixel(ctx, 98,25, 0,255,0,255);
       //comparePixel(ctx, 1,48, 0,255,0,255);
       //comparePixel(ctx, 50,48, 0,255,0,255);
       //comparePixel(ctx, 98,48, 0,255,0,255);

       ctx.reset();
       var tol = 1; // tolerance to avoid antialiasing artifacts

       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       ctx.fillStyle = '#f00';
       ctx.beginPath();
       ctx.moveTo(30+tol, 40);
       ctx.lineTo(110, -20+tol);
       ctx.lineTo(110, 100-tol);
       ctx.fill();

       g = ctx.createRadialGradient(30+10*5/2, 40, 10*3/2, 30+10*15/4, 40, 10*9/4);
       g.addColorStop(0, '#0f0');
       g.addColorStop(1, '#0f0');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 50,1, 0,255,0,255);
       //comparePixel(ctx, 98,1, 0,255,0,255);
       //comparePixel(ctx, 1,25, 0,255,0,255,16);
       //comparePixel(ctx, 50,25, 0,255,0,255);
       //comparePixel(ctx, 98,25, 0,255,0,255);
       //comparePixel(ctx, 1,48, 0,255,0,255);
       //comparePixel(ctx, 50,48, 0,255,0,255);
       //comparePixel(ctx, 98,48, 0,255,0,255);

       ctx.reset();
       var tol = 1; // tolerance to avoid antialiasing artifacts

       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       g = ctx.createRadialGradient(30+10*5/2, 40, 10*3/2, 30+10*15/4, 40, 10*9/4);
       g.addColorStop(0, '#f00');
       g.addColorStop(1, '#f00');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);

       ctx.fillStyle = '#0f0';
       ctx.beginPath();
       ctx.moveTo(30-tol, 40);
       ctx.lineTo(110, -20-tol);
       ctx.lineTo(110, 100+tol);
       ctx.fill();

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 50,1, 0,255,0,255);
       //comparePixel(ctx, 98,1, 0,255,0,255);
       //comparePixel(ctx, 1,25, 0,255,0,255,16);
       //comparePixel(ctx, 50,25, 0,255,0,255);
       //comparePixel(ctx, 98,25, 0,255,0,255);
       //comparePixel(ctx, 1,48, 0,255,0,255);
       //comparePixel(ctx, 50,48, 0,255,0,255);
       //comparePixel(ctx, 98,48, 0,255,0,255);

       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       g = ctx.createRadialGradient(230, 25, 100, 100, 25, 101);
       g.addColorStop(0, '#f00');
       g.addColorStop(1, '#0f0');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 50,1, 0,255,0,255);
       //comparePixel(ctx, 98,1, 0,255,0,255);
       //comparePixel(ctx, 1,25, 0,255,0,255,16);
       //comparePixel(ctx, 50,25, 0,255,0,255);
       //comparePixel(ctx, 98,25, 0,255,0,255);
       //comparePixel(ctx, 1,48, 0,255,0,255);
       //comparePixel(ctx, 50,48, 0,255,0,255);
       //comparePixel(ctx, 98,48, 0,255,0,255);

       ctx.reset();
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       g = ctx.createRadialGradient(50, 25, 20, 50, 25, 20);
       g.addColorStop(0, '#f00');
       g.addColorStop(1, '#f00');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 50,1, 0,255,0,255);
       //comparePixel(ctx, 98,1, 0,255,0,255);
       //comparePixel(ctx, 1,25, 0,255,0,255,16);
       //comparePixel(ctx, 50,25, 0,255,0,255);
       //comparePixel(ctx, 98,25, 0,255,0,255);
       //comparePixel(ctx, 1,48, 0,255,0,255);
       //comparePixel(ctx, 50,48, 0,255,0,255);
       //comparePixel(ctx, 98,48, 0,255,0,255);

       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       g = ctx.createRadialGradient(50, 25, 100, 50, 25, 200);
       g.addColorStop(0, '#0f0');
       g.addColorStop(1, '#f00');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 50,1, 0,255,0,255);
       //comparePixel(ctx, 98,1, 0,255,0,255);
       //comparePixel(ctx, 1,25, 0,255,0,255,16);
       //comparePixel(ctx, 50,25, 0,255,0,255);
       //comparePixel(ctx, 98,25, 0,255,0,255);
       //comparePixel(ctx, 1,48, 0,255,0,255);
       //comparePixel(ctx, 50,48, 0,255,0,255);
       //comparePixel(ctx, 98,48, 0,255,0,255);

       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       g = ctx.createRadialGradient(50, 25, 200, 50, 25, 100);
       g.addColorStop(0, '#f00');
       g.addColorStop(1, '#0f0');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 50,1, 0,255,0,255);
       //comparePixel(ctx, 98,1, 0,255,0,255);
       //comparePixel(ctx, 1,25, 0,255,0,255,16);
       //comparePixel(ctx, 50,25, 0,255,0,255);
       //comparePixel(ctx, 98,25, 0,255,0,255);
       //comparePixel(ctx, 1,48, 0,255,0,255);
       //comparePixel(ctx, 50,48, 0,255,0,255);
       //comparePixel(ctx, 98,48, 0,255,0,255);

       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);

       g = ctx.createRadialGradient(50, 25, 200, 50, 25, 100);
       g.addColorStop(0, '#f00');
       g.addColorStop(0.993, '#f00');
       g.addColorStop(1, '#0f0');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 50,1, 0,255,0,255);
       //comparePixel(ctx, 98,1, 0,255,0,255);
       //comparePixel(ctx, 1,25, 0,255,0,255,16);
       //comparePixel(ctx, 50,25, 0,255,0,255);
       //comparePixel(ctx, 98,25, 0,255,0,255);
       //comparePixel(ctx, 1,48, 0,255,0,255);
       //comparePixel(ctx, 50,48, 0,255,0,255);
       //comparePixel(ctx, 98,48, 0,255,0,255);

       ctx.reset();
       try { var err = false;
         ctx.createRadialGradient(0, 0, -0.1, 0, 0, 1);
       } catch (e) { if (e.code != DOMException.INDEX_SIZE_ERR) fail("Failed assertion: expected exception of type INDEX_SIZE_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type INDEX_SIZE_ERR: ctx.createRadialGradient(0, 0, -0.1, 0, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, 1, 0, 0, -0.1);
       } catch (e) { if (e.code != DOMException.INDEX_SIZE_ERR) fail("Failed assertion: expected exception of type INDEX_SIZE_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type INDEX_SIZE_ERR: ctx.createRadialGradient(0, 0, 1, 0, 0, -0.1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, -0.1, 0, 0, -0.1);
       } catch (e) { if (e.code != DOMException.INDEX_SIZE_ERR) fail("Failed assertion: expected exception of type INDEX_SIZE_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type INDEX_SIZE_ERR: ctx.createRadialGradient(0, 0, -0.1, 0, 0, -0.1)"); }


       ctx.reset();


       try { var err = false;
         ctx.createRadialGradient(Infinity, 0, 1, 0, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, 0, 1, 0, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(-Infinity, 0, 1, 0, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(-Infinity, 0, 1, 0, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(NaN, 0, 1, 0, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(NaN, 0, 1, 0, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, Infinity, 1, 0, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, Infinity, 1, 0, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, -Infinity, 1, 0, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, -Infinity, 1, 0, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, NaN, 1, 0, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, NaN, 1, 0, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, Infinity, 0, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, Infinity, 0, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, -Infinity, 0, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, -Infinity, 0, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, NaN, 0, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, NaN, 0, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, 1, Infinity, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, 1, Infinity, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, 1, -Infinity, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, 1, -Infinity, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, 1, NaN, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, 1, NaN, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, 1, 0, Infinity, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, 1, 0, Infinity, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, 1, 0, -Infinity, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, 1, 0, -Infinity, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, 1, 0, NaN, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, 1, 0, NaN, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, 1, 0, 0, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, 1, 0, 0, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, 1, 0, 0, -Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, 1, 0, 0, -Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, 1, 0, 0, NaN);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, 1, 0, 0, NaN)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, Infinity, 1, 0, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, Infinity, 1, 0, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, Infinity, Infinity, 0, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, Infinity, Infinity, 0, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, Infinity, Infinity, Infinity, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, Infinity, Infinity, Infinity, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, Infinity, Infinity, Infinity, Infinity, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, Infinity, Infinity, Infinity, Infinity, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, Infinity, Infinity, Infinity, Infinity, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, Infinity, Infinity, Infinity, Infinity, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, Infinity, Infinity, Infinity, 0, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, Infinity, Infinity, Infinity, 0, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, Infinity, Infinity, 0, Infinity, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, Infinity, Infinity, 0, Infinity, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, Infinity, Infinity, 0, Infinity, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, Infinity, Infinity, 0, Infinity, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, Infinity, Infinity, 0, 0, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, Infinity, Infinity, 0, 0, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, Infinity, 1, Infinity, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, Infinity, 1, Infinity, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, Infinity, 1, Infinity, Infinity, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, Infinity, 1, Infinity, Infinity, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, Infinity, 1, Infinity, Infinity, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, Infinity, 1, Infinity, Infinity, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, Infinity, 1, Infinity, 0, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, Infinity, 1, Infinity, 0, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, Infinity, 1, 0, Infinity, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, Infinity, 1, 0, Infinity, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, Infinity, 1, 0, Infinity, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, Infinity, 1, 0, Infinity, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, Infinity, 1, 0, 0, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, Infinity, 1, 0, 0, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, 0, Infinity, 0, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, 0, Infinity, 0, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, 0, Infinity, Infinity, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, 0, Infinity, Infinity, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, 0, Infinity, Infinity, Infinity, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, 0, Infinity, Infinity, Infinity, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, 0, Infinity, Infinity, Infinity, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, 0, Infinity, Infinity, Infinity, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, 0, Infinity, Infinity, 0, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, 0, Infinity, Infinity, 0, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, 0, Infinity, 0, Infinity, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, 0, Infinity, 0, Infinity, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, 0, Infinity, 0, Infinity, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, 0, Infinity, 0, Infinity, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, 0, Infinity, 0, 0, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, 0, Infinity, 0, 0, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, 0, 1, Infinity, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, 0, 1, Infinity, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, 0, 1, Infinity, Infinity, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, 0, 1, Infinity, Infinity, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, 0, 1, Infinity, Infinity, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, 0, 1, Infinity, Infinity, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, 0, 1, Infinity, 0, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, 0, 1, Infinity, 0, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, 0, 1, 0, Infinity, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, 0, 1, 0, Infinity, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, 0, 1, 0, Infinity, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, 0, 1, 0, Infinity, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(Infinity, 0, 1, 0, 0, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(Infinity, 0, 1, 0, 0, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(0, Infinity, Infinity, 0, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, Infinity, Infinity, 0, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, Infinity, Infinity, Infinity, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, Infinity, Infinity, Infinity, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, Infinity, Infinity, Infinity, Infinity, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, Infinity, Infinity, Infinity, Infinity, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, Infinity, Infinity, Infinity, Infinity, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, Infinity, Infinity, Infinity, Infinity, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(0, Infinity, Infinity, Infinity, 0, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, Infinity, Infinity, Infinity, 0, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(0, Infinity, Infinity, 0, Infinity, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, Infinity, Infinity, 0, Infinity, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, Infinity, Infinity, 0, Infinity, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, Infinity, Infinity, 0, Infinity, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(0, Infinity, Infinity, 0, 0, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, Infinity, Infinity, 0, 0, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(0, Infinity, 1, Infinity, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, Infinity, 1, Infinity, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, Infinity, 1, Infinity, Infinity, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, Infinity, 1, Infinity, Infinity, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, Infinity, 1, Infinity, Infinity, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, Infinity, 1, Infinity, Infinity, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(0, Infinity, 1, Infinity, 0, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, Infinity, 1, Infinity, 0, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(0, Infinity, 1, 0, Infinity, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, Infinity, 1, 0, Infinity, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, Infinity, 1, 0, Infinity, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, Infinity, 1, 0, Infinity, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(0, Infinity, 1, 0, 0, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, Infinity, 1, 0, 0, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, Infinity, Infinity, 0, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, Infinity, Infinity, 0, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, Infinity, Infinity, Infinity, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, Infinity, Infinity, Infinity, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, Infinity, Infinity, Infinity, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, Infinity, Infinity, Infinity, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, Infinity, Infinity, 0, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, Infinity, Infinity, 0, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, Infinity, 0, Infinity, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, Infinity, 0, Infinity, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, Infinity, 0, Infinity, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, Infinity, 0, Infinity, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, Infinity, 0, 0, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, Infinity, 0, 0, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, 1, Infinity, Infinity, 1);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, 1, Infinity, Infinity, 1)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, 1, Infinity, Infinity, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, 1, Infinity, Infinity, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, 1, Infinity, 0, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, 1, Infinity, 0, Infinity)"); }
       try { var err = false;
         ctx.createRadialGradient(0, 0, 1, 0, Infinity, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createRadialGradient(0, 0, 1, 0, Infinity, Infinity)"); }


       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       g = ctx.createRadialGradient(200, 25, 10, 200, 25, 20);
       g.addColorStop(0, '#f00');
       g.addColorStop(1, '#0f0');
       ctx.fillStyle = g;ctx.fillRect(0, 0, 100, 50);

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 50,1, 0,255,0,255);//comparePixel(ctx, 98,1, 0,255,0,255);
       //comparePixel(ctx, 1,25, 0,255,0,255,16);
       //comparePixel(ctx, 50,25, 0,255,0,255);
       //comparePixel(ctx, 98,25, 0,255,0,255);//comparePixel(ctx, 1,48, 0,255,0,255);
       //comparePixel(ctx, 50,48, 0,255,0,255);
       //comparePixel(ctx, 98,48, 0,255,0,255);

       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       g = ctx.createRadialGradient(200, 25, 20, 200, 25, 10);
       g.addColorStop(0, '#0f0');
       g.addColorStop(1, '#f00');
       ctx.fillStyle = g;ctx.fillRect(0, 0, 100, 50);

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 50,1, 0,255,0,255);//comparePixel(ctx, 98,1, 0,255,0,255);
       //comparePixel(ctx, 1,25, 0,255,0,255,16);
       //comparePixel(ctx, 50,25, 0,255,0,255);
       //comparePixel(ctx, 98,25, 0,255,0,255);//comparePixel(ctx, 1,48, 0,255,0,255);
       //comparePixel(ctx, 50,48, 0,255,0,255);
       //comparePixel(ctx, 98,48, 0,255,0,255);

       ctx.reset();
       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       g = ctx.createRadialGradient(200, 25, 20, 200, 25, 10);
       g.addColorStop(0, '#0f0');
       g.addColorStop(0.001, '#f00');
       g.addColorStop(1, '#f00');ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);

       //comparePixel(ctx, 1,1, 0,255,0,255);//comparePixel(ctx, 50,1, 0,255,0,255);
       //comparePixel(ctx, 98,1, 0,255,0,255);
       //comparePixel(ctx, 1,25, 0,255,0,255,16);
       //comparePixel(ctx, 50,25, 0,255,0,255);//comparePixel(ctx, 98,25, 0,255,0,255);
       //comparePixel(ctx, 1,48, 0,255,0,255);
       //comparePixel(ctx, 50,48, 0,255,0,255);
       //comparePixel(ctx, 98,48, 0,255,0,255);

       ctx.reset();
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);
       g = ctx.createRadialGradient(150, 25, 50, 200, 25, 100);
       g.addColorStop(0, '#f00');
       g.addColorStop(1, '#f00');
       ctx.fillStyle = g;ctx.fillRect(0, 0, 100, 50);

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 50,1, 0,255,0,255);//comparePixel(ctx, 98,1, 0,255,0,255);
       //comparePixel(ctx, 1,25, 0,255,0,255,16);
       //comparePixel(ctx, 50,25, 0,255,0,255);
       //comparePixel(ctx, 98,25, 0,255,0,255);//comparePixel(ctx, 1,48, 0,255,0,255);
       //comparePixel(ctx, 50,48, 0,255,0,255);
       //comparePixel(ctx, 98,48, 0,255,0,255);

       ctx.reset();

       ctx.fillStyle = '#f00';
       ctx.fillRect(0, 0, 100, 50);
       g = ctx.createRadialGradient(-80, 25, 70, 0, 25, 150);
       g.addColorStop(0, '#f00');
       g.addColorStop(0.01, '#0f0');
       g.addColorStop(0.99, '#0f0');g.addColorStop(1, '#f00');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);
       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 50,1, 0,255,0,255);
       //comparePixel(ctx, 98,1, 0,255,0,255);
       //comparePixel(ctx, 1,25, 0,255,0,255,16);//comparePixel(ctx, 50,25, 0,255,0,255);
       //comparePixel(ctx, 98,25, 0,255,0,255);
       //comparePixel(ctx, 1,48, 0,255,0,255);
       //comparePixel(ctx, 50,48, 0,255,0,255);//comparePixel(ctx, 98,48, 0,255,0,255);

       ctx.reset();
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);
       g = ctx.createRadialGradient(120, -15, 25, 140, -30, 50);
       g.addColorStop(0, '#f00');
       g.addColorStop(1, '#f00');
       ctx.fillStyle = g;ctx.fillRect(0, 0, 100, 50);

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 50,1, 0,255,0,255);//comparePixel(ctx, 98,1, 0,255,0,255);
       //comparePixel(ctx, 1,25, 0,255,0,255,16);
       //comparePixel(ctx, 50,25, 0,255,0,255);
       //comparePixel(ctx, 98,25, 0,255,0,255);//comparePixel(ctx, 1,48, 0,255,0,255);
       //comparePixel(ctx, 50,48, 0,255,0,255);
       //comparePixel(ctx, 98,48, 0,255,0,255);

       ctx.reset();
       g = ctx.createRadialGradient(0, 0, 0, 0, 0, 11.2);
       g.addColorStop(0, '#0f0');
       g.addColorStop(0.5, '#0f0');g.addColorStop(0.51, '#f00');
       g.addColorStop(1, '#f00');
       ctx.fillStyle = g;
       ctx.translate(50, 25);ctx.scale(10, 10);
       ctx.fillRect(-5, -2.5, 10, 5);
       //comparePixel(ctx, 25,25, 0,255,0,255);
       //comparePixel(ctx, 50,25, 0,255,0,255);//comparePixel(ctx, 75,25, 0,255,0,255);

       ctx.reset();
       ctx.translate(100, 0);
       g = ctx.createRadialGradient(0, 0, 0, 0, 0, 11.2);
       g.addColorStop(0, '#0f0');g.addColorStop(0.5, '#0f0');
       g.addColorStop(0.51, '#f00');
       g.addColorStop(1, '#f00');
       ctx.fillStyle = g;ctx.translate(-50, 25);
       ctx.scale(10, 10);
       ctx.fillRect(-5, -2.5, 10, 5);
       //comparePixel(ctx, 25,25, 0,255,0,255);//comparePixel(ctx, 50,25, 0,255,0,255);
       //comparePixel(ctx, 75,25, 0,255,0,255);


       ctx.reset();
       g = ctx.createRadialGradient(0, 0, 0, 0, 0, 11.2);
       g.addColorStop(0, '#0f0');
       g.addColorStop(0.5, '#0f0');g.addColorStop(0.51, '#f00');
       g.addColorStop(1, '#f00');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);ctx.translate(50, 25);
       ctx.scale(10, 10);
       ctx.fillRect(-5, -2.5, 10, 5);
       //comparePixel(ctx, 25,25, 0,255,0,255);//comparePixel(ctx, 50,25, 0,255,0,255);
       //comparePixel(ctx, 75,25, 0,255,0,255);


  }
   function test_linear(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       try { var err = false;
         ctx.createLinearGradient(Infinity, 0, 1, 0);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(Infinity, 0, 1, 0)"); }
       try { var err = false;
         ctx.createLinearGradient(-Infinity, 0, 1, 0);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(-Infinity, 0, 1, 0)"); }
       try { var err = false;
         ctx.createLinearGradient(NaN, 0, 1, 0);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(NaN, 0, 1, 0)"); }
       try { var err = false;
         ctx.createLinearGradient(0, Infinity, 1, 0);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(0, Infinity, 1, 0)"); }
       try { var err = false;
         ctx.createLinearGradient(0, -Infinity, 1, 0);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(0, -Infinity, 1, 0)"); }
       try { var err = false;
         ctx.createLinearGradient(0, NaN, 1, 0);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(0, NaN, 1, 0)"); }
       try { var err = false;
         ctx.createLinearGradient(0, 0, Infinity, 0);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(0, 0, Infinity, 0)"); }
       try { var err = false;
         ctx.createLinearGradient(0, 0, -Infinity, 0);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(0, 0, -Infinity, 0)"); }
       try { var err = false;
         ctx.createLinearGradient(0, 0, NaN, 0);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(0, 0, NaN, 0)"); }
       try { var err = false;
         ctx.createLinearGradient(0, 0, 1, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(0, 0, 1, Infinity)"); }
       try { var err = false;
         ctx.createLinearGradient(0, 0, 1, -Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(0, 0, 1, -Infinity)"); }
       try { var err = false;
         ctx.createLinearGradient(0, 0, 1, NaN);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(0, 0, 1, NaN)"); }
       try { var err = false;
         ctx.createLinearGradient(Infinity, Infinity, 1, 0);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(Infinity, Infinity, 1, 0)"); }
       try { var err = false;
         ctx.createLinearGradient(Infinity, Infinity, Infinity, 0);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(Infinity, Infinity, Infinity, 0)"); }
       try { var err = false;
         ctx.createLinearGradient(Infinity, Infinity, Infinity, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(Infinity, Infinity, Infinity, Infinity)"); }
       try { var err = false;
         ctx.createLinearGradient(Infinity, Infinity, 1, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(Infinity, Infinity, 1, Infinity)"); }
       try { var err = false;
         ctx.createLinearGradient(Infinity, 0, Infinity, 0);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(Infinity, 0, Infinity, 0)"); }
       try { var err = false;
         ctx.createLinearGradient(Infinity, 0, Infinity, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(Infinity, 0, Infinity, Infinity)"); }
       try { var err = false;
         ctx.createLinearGradient(Infinity, 0, 1, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(Infinity, 0, 1, Infinity)"); }
       try { var err = false;
         ctx.createLinearGradient(0, Infinity, Infinity, 0);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(0, Infinity, Infinity, 0)"); }
       try { var err = false;
         ctx.createLinearGradient(0, Infinity, Infinity, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(0, Infinity, Infinity, Infinity)"); }
       try { var err = false;
         ctx.createLinearGradient(0, Infinity, 1, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(0, Infinity, 1, Infinity)"); }
       try { var err = false;
         ctx.createLinearGradient(0, 0, Infinity, Infinity);
       } catch (e) { if (e.code != DOMException.NOT_SUPPORTED_ERR) fail("Failed assertion: expected exception of type NOT_SUPPORTED_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type NOT_SUPPORTED_ERR: ctx.createLinearGradient(0, 0, Infinity, Infinity)"); }

       ctx.reset();
       var g = ctx.createLinearGradient(0, 0, 200, 0);
       g.addColorStop(0, '#f00');
       g.addColorStop(0.25, '#0f0');
       g.addColorStop(0.75, '#0f0');
       g.addColorStop(1, '#f00');
       ctx.fillStyle = g;
       ctx.translate(-50, 0);
       ctx.fillRect(50, 0, 100, 50);
       comparePixel(ctx, 25,25, 0,255,0,255);
       comparePixel(ctx, 50,25, 0,255,0,255);
       comparePixel(ctx, 75,25, 0,255,0,255);

       ctx.reset();
       ctx.translate(100, 0);
       g = ctx.createLinearGradient(0, 0, 200, 0);
       g.addColorStop(0, '#f00');
       g.addColorStop(0.25, '#0f0');
       g.addColorStop(0.75, '#0f0');
       g.addColorStop(1, '#f00');
       ctx.fillStyle = g;
       ctx.translate(-150, 0);
       ctx.fillRect(50, 0, 100, 50);
       comparePixel(ctx, 25,25, 0,255,0,255);
       comparePixel(ctx, 50,25, 0,255,0,255);
       comparePixel(ctx, 75,25, 0,255,0,255);


       ctx.reset();
       g = ctx.createLinearGradient(0, 0, 200, 0);
       g.addColorStop(0, '#f00');
       g.addColorStop(0.25, '#0f0');
       g.addColorStop(0.75, '#0f0');
       g.addColorStop(1, '#f00');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);
       ctx.translate(-50, 0);
       ctx.fillRect(50, 0, 100, 50);

       if (canvas.renderTarget === Canvas.Image) //FIXME:broken on FramebufferObject
           comparePixel(ctx, 25,25, 0,255,0,255);
       comparePixel(ctx, 50,25, 0,255,0,255);
       comparePixel(ctx, 75,25, 0,255,0,255);

  }
   function test_object(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       var g1 = ctx.createLinearGradient(0, 0, 100, 0);
       var g2 = ctx.createLinearGradient(0, 0, 100, 0);
       ctx.fillStyle = g1;


       ctx.reset();
       var g = ctx.createLinearGradient(0, 0, 100, 0);
       try { var err = false;
         g.addColorStop(0, "");
       } catch (e) { if (e.code != DOMException.SYNTAX_ERR) fail("Failed assertion: expected exception of type SYNTAX_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type SYNTAX_ERR: g.addColorStop(0, \"\")"); }
       try { var err = false;
         g.addColorStop(0, 'undefined');
       } catch (e) { if (e.code != DOMException.SYNTAX_ERR) fail("Failed assertion: expected exception of type SYNTAX_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type SYNTAX_ERR: g.addColorStop(0, 'undefined')"); }


       ctx.reset();
       g = ctx.createLinearGradient(0, 0, 100, 0);
       try { var err = false;
         g.addColorStop(-1, '#000');
       } catch (e) { if (e.code != DOMException.INDEX_SIZE_ERR) fail("Failed assertion: expected exception of type INDEX_SIZE_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type INDEX_SIZE_ERR: g.addColorStop(-1, '#000')"); }
       try { var err = false;
         g.addColorStop(2, '#000');
       } catch (e) { if (e.code != DOMException.INDEX_SIZE_ERR) fail("Failed assertion: expected exception of type INDEX_SIZE_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type INDEX_SIZE_ERR: g.addColorStop(2, '#000')"); }
       try { var err = false;
         g.addColorStop(Infinity, '#000');
       } catch (e) { if (e.code != DOMException.INDEX_SIZE_ERR) fail("Failed assertion: expected exception of type INDEX_SIZE_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type INDEX_SIZE_ERR: g.addColorStop(Infinity, '#000')"); }
       try { var err = false;
         g.addColorStop(-Infinity, '#000');
       } catch (e) { if (e.code != DOMException.INDEX_SIZE_ERR) fail("Failed assertion: expected exception of type INDEX_SIZE_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type INDEX_SIZE_ERR: g.addColorStop(-Infinity, '#000')"); }
       try { var err = false;
         g.addColorStop(NaN, '#000');
       } catch (e) { if (e.code != DOMException.INDEX_SIZE_ERR) fail("Failed assertion: expected exception of type INDEX_SIZE_ERR, got: "+e.message); err = true; } finally { verify(err, "should throw exception of type INDEX_SIZE_ERR: g.addColorStop(NaN, '#000')"); }


       ctx.reset();
       g = ctx.createLinearGradient(-100, 0, 200, 0);
       g.addColorStop(0, '#f00');
       g.addColorStop(1, '#f00');
       ctx.fillStyle = g;
       g.addColorStop(0.1, '#0f0');
       g.addColorStop(0.9, '#0f0');
       ctx.fillRect(0, 0, 100, 50);
       //comparePixel(ctx, 50,25, 0,255,0,255,2);

       ctx.reset();
       ctx.fillStyle = '#0f0';
       ctx.fillRect(0, 0, 100, 50);

       g = ctx.createRadialGradient(120, 25, 10, 211, 25, 100);
       g.addColorStop(0, '#f00');
       g.addColorStop(1, '#f00');
       ctx.fillStyle = g;
       ctx.fillRect(0, 0, 100, 50);

       //comparePixel(ctx, 1,1, 0,255,0,255);
       //comparePixel(ctx, 50,1, 0,255,0,255);
       //comparePixel(ctx, 98,1, 0,255,0,255);
       //comparePixel(ctx, 1,25, 0,255,0,255,16);
       //comparePixel(ctx, 50,25, 0,255,0,255);
       //comparePixel(ctx, 98,25, 0,255,0,255);
       //comparePixel(ctx, 1,48, 0,255,0,255);
       //comparePixel(ctx, 50,48, 0,255,0,255);
       //comparePixel(ctx, 98,48, 0,255,0,255);


  }

   function test_conical(row) {
       var canvas = createCanvasObject(row);
       var ctx = canvas.getContext('2d');
       ctx.reset();
       var g = ctx.createConicalGradient(10, 10, 50);
       //TODO
   }
}
