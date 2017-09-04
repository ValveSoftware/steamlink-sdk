import QtQuick 2.0

Item {
    id: container
    width: 200
    height: 200

CanvasTestCase {
   id:testCase
   name: "canvas"
   function init_data() { return testData("2d"); }

   function test_canvasSize(row) {
       var c = createCanvasObject(row);
       verify(c);
       var ctx = c.getContext("2d");
       verify(ctx);

       tryCompare(c, "availableChangedCount", 1);
       //by default canvasSize is same with canvas' actual size
       // when canvas size changes, canvasSize should be changed as well.
       compare(c.canvasSize.width, c.width);
       compare(c.canvasSize.height, c.height);
       c.width = 20;
       compare(c.canvasSize.width, 20);
       compare(c.canvasSizeChangedCount, 1);
       c.height = 5;
       compare(c.canvasSizeChangedCount, 2);
       compare(c.canvasSize.height, 5);

       //change canvasSize manually, then canvasSize detaches from canvas
       //actual size.
       c.canvasSize.width = 100;
       compare(c.canvasSizeChangedCount, 3);
       compare(c.canvasSize.width, 100);
       compare(c.width, 20);
       c.canvasSize.height = 50;
       compare(c.canvasSizeChangedCount, 4);
       compare(c.canvasSize.height, 50);
       compare(c.height, 5);

       c.width = 10;
       compare(c.canvasSizeChangedCount, 4);
       compare(c.canvasSize.width, 100);
       compare(c.canvasSize.height, 50);

       c.height = 10;

       compare(c.canvasSizeChangedCount, 4);
       compare(c.canvasSize.width, 100);
       compare(c.canvasSize.height, 50);
       c.destroy();
  }
   function test_tileSize(row) {
       var c = createCanvasObject(row);
       verify(c);
       var ctx = c.getContext("2d");
       verify(ctx);
       tryCompare(c, "availableChangedCount", 1);

       compare(c.tileSize.width, c.width);
       compare(c.tileSize.height, c.height);
       c.width = 20;
       compare(c.tileSize.width, 20);
       compare(c.tileSizeChangedCount, 1);
       c.height = 5;
       compare(c.tileSizeChangedCount, 2);
       compare(c.tileSize.height, 5);

       c.tileSize.width = 100;
       compare(c.tileSizeChangedCount, 3);
       compare(c.tileSize.width, 100);
       compare(c.width, 20);
       c.tileSize.height = 50;
       compare(c.tileSizeChangedCount, 4);
       compare(c.tileSize.height, 50);
       compare(c.height, 5);

       c.width = 10;
       compare(c.tileSizeChangedCount, 4);
       compare(c.tileSize.width, 100);
       compare(c.tileSize.height, 50);

       c.height = 10;
       compare(c.tileSizeChangedCount, 4);
       compare(c.tileSize.width, 100);
       compare(c.tileSize.height, 50);
       c.destroy();

   }

   function test_canvasWindow(row) {
       var c = createCanvasObject(row);
       verify(c);
       var ctx = c.getContext("2d");
       verify(ctx);

       tryCompare(c, "availableChangedCount", 1);
       compare(c.canvasWindow.x, 0);
       compare(c.canvasWindow.y, 0);
       compare(c.canvasWindow.width, c.width);
       compare(c.canvasWindow.height, c.height);

       c.width = 20;
       compare(c.canvasWindow.width, 20);
       compare(c.canvasWindowChangedCount, 1);
       c.height = 5;
       compare(c.canvasWindowChangedCount, 2);
       compare(c.canvasWindow.height, 5);

       c.canvasWindow.x = 5;
       c.canvasWindow.y = 6;
       c.canvasWindow.width = 10;
       c.canvasWindow.height =20;
       compare(c.canvasWindowChangedCount, 6);
       compare(c.canvasWindow.width, 10);
       compare(c.canvasWindow.height, 20);
       compare(c.canvasWindow.x, 5);
       compare(c.canvasWindow.y, 6);
       c.destroy();

  }

   function test_save(row) {
       var c = createCanvasObject(row);
       verify(c);
       var ctx = c.getContext("2d");

       tryCompare(c, "availableChangedCount", 1);

       c.requestPaint();
       verify(c.save("c.png"));
       c.loadImage("c.png");
       wait(200);
       verify(c.isImageLoaded("c.png"));
       verify(!c.isImageLoading("c.png"));
       verify(!c.isImageError("c.png"));
       c.destroy();
  }

   function test_toDataURL(row) {
       var c = createCanvasObject(row);
       verify(c);
       var ctx = c.getContext("2d");
       verify(ctx);

       tryCompare(c, "availableChangedCount", 1);

       var imageTypes = [
                   {mimeType:"image/png"},
                   {mimeType:"image/bmp"},
                   {mimeType:"image/jpeg"},
                   {mimeType:"image/x-portable-pixmap"},
                   //{mimeType:"image/tiff"}, QTBUG-23980
                   {mimeType:"image/xpm"},
                  ];
       for (var i = 0; i < imageTypes.length; i++) {
           ctx.fillStyle = "red";
           ctx.fillRect(0, 0, c.width, c.height);

           var dataUrl = c.toDataURL();
           verify(dataUrl !== "data:,");
           dataUrl = c.toDataURL("image/invalid");
           verify(dataUrl === "data:,");

           dataUrl = c.toDataURL(imageTypes[i].mimeType);
           verify(dataUrl !== "data:,");

           ctx.save();
           ctx.fillStyle = "blue";
           ctx.fillRect(0, 0, c.width, c.height);
           ctx.restore();

           var dataUrl2 = c.toDataURL(imageTypes[i].mimeType);
           verify (dataUrl2 !== "data:,");
           verify (dataUrl2 !== dataUrl);
       }
       c.destroy();

  }
   function test_paint(row) {
       var c = createCanvasObject(row);
       verify(c);
       var ctx = c.getContext("2d");
       tryCompare(c, "availableChangedCount", 1);
       //scene graph could be available immediately
       //in this case, we force waiting a short while until the init paint finished
       tryCompare(c, "paintedCount", 1);
       ctx.fillRect(0, 0, c.width, c.height);
       c.toDataURL();
       tryCompare(c, "paintedCount", 2);
       tryCompare(c, "paintCount", 1);
       // implicit repaint when visible and resized
       testCase.visible = true;
       c.width += 1;
       c.height += 1;
       tryCompare(c, "paintCount", 2);
       tryCompare(c, "paintedCount", 2);
       // allow explicit repaint even when hidden
       testCase.visible = false;
       c.requestPaint();
       tryCompare(c, "paintCount", 3);
       tryCompare(c, "paintedCount", 2);
       // no implicit repaint when resized but hidden
       c.width += 1;
       c.height += 1;
       waitForRendering(c);
       compare(c.paintCount, 3);
       tryCompare(c, "paintedCount", 2);
       c.destroy();
  }
   function test_loadImage(row) {
       var c = createCanvasObject(row);
       verify(c);
       var ctx = c.getContext("2d");
       verify(ctx);

       tryCompare(c, "availableChangedCount", 1);

       verify(!c.isImageLoaded("red.png"));
       c.loadImage("red.png");
       wait(200);
       verify(c.isImageLoaded("red.png"));
       verify(!c.isImageLoading("red.png"));
       verify(!c.isImageError("red.png"));

       c.unloadImage("red.png");
       verify(!c.isImageLoaded("red.png"));
       verify(!c.isImageLoading("red.png"));
       verify(!c.isImageError("red.png"));
       c.destroy();

  }

   function test_getContext(row) {
       var c = createCanvasObject(row);
       verify(c);
       var ctx = c.getContext("2d");
       verify(ctx);
       tryCompare(c, "availableChangedCount", 1);

       compare(ctx.canvas, c);
       ctx = c.getContext('2d');
       verify(ctx);
       compare(ctx.canvas, c);
       ctx = c.getContext('2D');
       verify(ctx);
       compare(ctx.canvas, c);
       ignoreWarning(Qt.resolvedUrl("CanvasComponent.qml") + ":6:9: QML Canvas: Canvas already initialized with a different context type");
       ctx = c.getContext('invalid');
       verify(!ctx);
       c.destroy();

  }

    Image {
        id: image
        source: "anim-gr.png"
    }

    /*
        Ensures that extra arguments to functions are ignored,
        by checking that drawing, clearing, etc. still occurs.
    */
    function test_extraArgumentsIgnored_data() {
        var extra = 0;
        return [
            {
                tag: "arc",
                test: function(ctx) {
                    ctx.arc(10, 10, 5, 0, Math.PI * 2, true, extra);
                    ctx.fill();
                    comparePixel(ctx, 10, 10, 255, 0, 0, 255);
                }
            },
            {
                tag: "arcTo",
                test: function(ctx) {
                    ctx.translate(-50, -25);
                    ctx.moveTo(20,20);
                    ctx.arcTo(150, 20, 100, 70, 50, extra);
                    ctx.fill();
                    comparePixel(ctx, 0, 0, 255, 0, 0, 255);
                }
            },
            {
                tag: "bezierCurveTo",
                test: function(ctx) {
                    ctx.beginPath();
                    ctx.moveTo(-20, -20);
                    ctx.bezierCurveTo(20, 100, 100, 100, 100, 20, extra);
                    ctx.fill();
                    comparePixel(ctx, 0, 0, 255, 0, 0, 255);
                }
            },
            {
                tag: "clearRect",
                test: function(ctx) {
                    ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);
                    ctx.clearRect(0, 0, 10, 10, extra);
                    comparePixel(ctx, 0, 0, 0, 0, 0, 0);
                }
            },
            {
                tag: "createConicalGradient",
                test: function(ctx) {
                    verify(ctx.createConicalGradient(0, 0, 0, extra) !== ctx);
                }
            },
            {
                tag: "createLinearGradient",
                test: function(ctx) {
                    verify(ctx.createLinearGradient(0, 0, 10, 10, extra) !== ctx);
                }
            },
            {
                tag: "createRadialGradient",
                test: function(ctx) {
                    verify(ctx.createRadialGradient(0, 0, 10, 20, 20, 10, extra) !== ctx);
                }
            },
            {
                tag: "createPattern-image",
                test: function(ctx) {
                    verify(ctx.createPattern(image, "repeat", extra) !== undefined);
                }
            },
            {
                tag: "createPattern-color",
                test: function(ctx) {
                    verify(ctx.createPattern("red", Qt.SolidPattern, extra) !== undefined);
                }
            },
            {
                tag: "drawImage-9-args",
                test: function(ctx) {
                    ctx.drawImage(image, 0, 0, image.sourceSize.width, image.sourceSize.height,
                        0, 0, image.sourceSize.width, image.sourceSize.height, extra);
                    comparePixel(ctx, 10, 10, 0, 255, 0, 255);
                }
            },
            {
                tag: "drawImage-5-args",
                test: function(ctx) {
                    ctx.drawImage(image, 0, 0, image.sourceSize.width, image.sourceSize.height, extra);
                    comparePixel(ctx, 10, 10, 0, 255, 0, 255);
                }
            },
            {
                tag: "drawImage-3-args",
                test: function(ctx) {
                    ctx.drawImage(image, 0, 0, extra);
                    comparePixel(ctx, 10, 10, 0, 255, 0, 255);
                }
            },
            {
                tag: "ellipse",
                test: function(ctx) {
                    ctx.ellipse(0, 0, 10, 10);
                    ctx.fill();
                    comparePixel(ctx, 5, 5, 255, 0, 0, 255);
                }
            },
            {
                tag: "fillRect",
                test: function(ctx) {
                    ctx.fillRect(0, 0, 10, 10, extra);
                    comparePixel(ctx, 0, 0, 255, 0, 0, 255, 200);
                }
            },
            {
                tag: "fillText",
                test: function(ctx) {
                    ctx.font = "100px sans-serif";
                    ctx.fillText("Hello", -10, 10, extra);
                    for (var x = 0; x < 100; ++x) {
                        var c = ctx.getImageData(x,0,1,1).data;
                        if (c[0] === 255 && c[1] === 0 && c[2] === 0 && c[3] === 255)
                            return;
                    }
                    qtest_fail("No red pixel found");
                }
            },
            {
                tag: "getImageData",
                test: function(ctx) {
                    verify(ctx.getImageData(0, 0, 1, 1, extra) !== null);
                }
            },
            {
                tag: "isPointInPath",
                test: function(ctx) {
                    ctx.moveTo(0, 0);
                    ctx.lineTo(10, 10);
                    verify(ctx.isPointInPath(0, 0, extra));
                }
            },
            {
                tag: "lineTo",
                test: function(ctx) {
                    ctx.lineWidth = 5;
                    ctx.moveTo(0, 0);
                    ctx.lineTo(10, 10, extra);
                    ctx.stroke();
                    comparePixel(ctx, 0, 0, 255, 0, 0, 255);
                }
            },
            {
                tag: "measureText",
                test: function(ctx) {
                    var textMetrics = ctx.measureText("Hello", extra);
                    verify(textMetrics !== undefined);
                    verify(textMetrics.width > 0);
                }
            },
            {
                tag: "moveTo",
                test: function(ctx) {
                    ctx.lineWidth = 5;
                    ctx.moveTo(10, 10, extra);
                    ctx.lineTo(20, 20, extra);
                    ctx.stroke();
                    comparePixel(ctx, 0, 0, 0, 0, 0, 0);
                    comparePixel(ctx, 10, 10, 255, 0, 0, 255);
                }
            },
            {
                tag: "putImageData",
                test: function(ctx) {
                    ctx.drawImage(image, 0, 0);
                    comparePixel(ctx, 0, 0, 0, 255, 0, 255);
                    var imageData = ctx.getImageData(0, 0, 1, 1);
                    // Swap green with red.
                    imageData.data[0] = 255;
                    imageData.data[1] = 0;
                    ctx.putImageData(imageData, 0, 0, 0, 0, ctx.canvas.width, ctx.canvas.height, extra);
                    comparePixel(ctx, 0, 0, 255, 0, 0, 255);
                }
            },
            {
                tag: "quadraticCurveTo",
                test: function(ctx) {
                    ctx.lineWidth = 5;
                    ctx.moveTo(0, 0);
                    ctx.quadraticCurveTo(20, 100, 100, 20, extra);
                    ctx.stroke();
                    comparePixel(ctx, 0, 0, 255, 0, 0, 255);
                }
            },
            {
                tag: "rect",
                test: function(ctx) {
                    ctx.rect(0, 0, 1, 1, extra);
                    ctx.fill();
                    comparePixel(ctx, 0, 0, 255, 0, 0, 255);
                }
            },
            {
                tag: "rotate",
                test: function(ctx) {
                    // If we don't rotate, it should be red in the middle.
                    comparePixel(ctx, 50, 50, 0, 0, 0, 0);
                    ctx.fillRect(50, 50, 1, 1);
                    comparePixel(ctx, 50, 50, 255, 0, 0, 255);

                    ctx.reset();
                    comparePixel(ctx, 50, 50, 0, 0, 0, 0);

                    // If we do rotate, it shouldn't be there.
                    ctx.rotate(Math.PI / 4, extra);
                    ctx.fillRect(50, 50, 1, 1);
                    comparePixel(ctx, 50, 50, 0, 0, 0, 0);
                }
            },
            {
                tag: "roundedRect",
                test: function(ctx) {
                    ctx.roundedRect(0, 0, 50, 50, 5, 5, extra);
                    ctx.fill();
                    comparePixel(ctx, 25, 25, 255, 0, 0, 255);
                }
            },
            {
                tag: "scale",
                test: function(ctx) {
                    // If we don't scale, it should be red in the middle.
                    comparePixel(ctx, 50, 50, 0, 0, 0, 0);
                    ctx.fillRect(50, 50, 1, 1);
                    comparePixel(ctx, 50, 50, 255, 0, 0, 255);

                    ctx.reset();
                    comparePixel(ctx, 50, 50, 0, 0, 0, 0);

                    // If we do scale, it shouldn't be there.
                    ctx.scale(1.25, 1.25, extra);
                    ctx.fillRect(50, 50, 1, 1);
                    comparePixel(ctx, 50, 50, 0, 0, 0, 0);
                }
            },
            {
                tag: "setTransform",
                test: function(ctx) {
                    // The same as the scale test, except with setTransform.
                    comparePixel(ctx, 50, 50, 0, 0, 0, 0);
                    ctx.fillRect(50, 50, 1, 1);
                    comparePixel(ctx, 50, 50, 255, 0, 0, 255);

                    ctx.reset();
                    comparePixel(ctx, 50, 50, 0, 0, 0, 0);

                    ctx.setTransform(1.25, 0, 0, 1.25, 0, 0, extra);
                    ctx.fillRect(50, 50, 1, 1);
                    comparePixel(ctx, 50, 50, 0, 0, 0, 0);
                }
            },
            {
                tag: "shear",
                test: function(ctx) {
                    comparePixel(ctx, 50, 50, 0, 0, 0, 0);
                    ctx.fillRect(50, 50, 1, 1);
                    comparePixel(ctx, 50, 50, 255, 0, 0, 255);

                    ctx.reset();
                    comparePixel(ctx, 50, 50, 0, 0, 0, 0);

                    ctx.shear(0.5, 0, extra);
                    ctx.fillRect(50, 50, 1, 1);
                    comparePixel(ctx, 50, 50, 0, 0, 0, 0);
                }
            },
            {
                tag: "strokeRect",
                test: function(ctx) {
                    ctx.strokeRect(0, 0, 10, 10, extra);
                    comparePixel(ctx, 0, 0, 255, 0, 0, 255, 200);
                }
            },
            {
                tag: "strokeText",
                test: function(ctx) {
                    ctx.font = "10px sans-serif";
                    ctx.strokeText("Hello", -1, 5, extra);
                    comparePixel(ctx, 0, 5, 255, 0, 0, 255, 200);
                }
            },
            {
                tag: "text",
                test: function(ctx) {
                    ctx.font = "100px sans-serif";
                    ctx.text(".", -15, 8, extra);
                    ctx.fill();
                    comparePixel(ctx, 0, 0, 255, 0, 0, 255, 200);
                }
            },
            {
                tag: "transform",
                test: function(ctx) {
                    comparePixel(ctx, 50, 50, 0, 0, 0, 0);
                    ctx.fillRect(50, 50, 1, 1);
                    comparePixel(ctx, 50, 50, 255, 0, 0, 255);

                    ctx.reset();
                    comparePixel(ctx, 50, 50, 0, 0, 0, 0);

                    ctx.transform(1.25, 0, 0, 1.25, 0, 0, extra);
                    ctx.fillRect(50, 50, 1, 1);
                    comparePixel(ctx, 50, 50, 0, 0, 0, 0);
                }
            },
            {
                tag: "translate",
                test: function(ctx) {
                    comparePixel(ctx, 50, 50, 0, 0, 0, 0);
                    ctx.fillRect(50, 50, 1, 1);
                    comparePixel(ctx, 50, 50, 255, 0, 0, 255);

                    ctx.reset();
                    comparePixel(ctx, 50, 50, 0, 0, 0, 0);

                    ctx.translate(1, 1, extra);
                    ctx.fillRect(50, 50, 1, 1);
                    comparePixel(ctx, 50, 50, 0, 0, 0, 0);
                }
            }
        ];
    }

    function test_extraArgumentsIgnored(data) {
        var canvas = Qt.createQmlObject("import QtQuick 2.3; Canvas { onPaint: {} }", testCase);
        verify(canvas);
        canvas.width = 100;
        canvas.height = 100;
        waitForRendering(canvas);

        var ctx = canvas.getContext("2d");

        ctx.beginPath();
        ctx.fillStyle = "red";
        ctx.strokeStyle = "red";
        data.test(ctx);

        canvas.destroy();
    }

    function test_getContextOnDestruction_data() {
        // We want to test all possible combinations deemed valid by the testcase,
        // but we can't test FramebufferObject due to difficulties ignoring the "available" warning.
        var allData = testData("2d");
        var ourData = [];

        for (var i = 0; i < allData.length; ++i) {
            if (allData[i].properties.renderTarget !== Canvas.FramebufferObject) {
                var row = allData[i].properties;
                row.tag = allData[i].tag;
                ourData.push(row);
            }
        }

        return ourData;
    }

    function test_getContextOnDestruction(data) {
        try {
            var canvasWindow = Qt.createQmlObject("
                import QtQuick 2.4\n
                import QtQuick.Window 2.2\n
                Window {\n
                    function test() {\n
                        loader.active = true\n
                        loader.active = false\n
                    }\n
                    Loader {\n
                        id: loader\n
                        active: false\n
                        sourceComponent: Canvas {\n
                            renderStrategy: " + renderStrategyToString(data.renderStrategy) + "\n
                            Component.onDestruction: getContext(\"2d\")
                        }\n
                    }\n
                }\n",
                testCase);
            verify(canvasWindow);
            canvasWindow.test();
            // Shouldn't crash when destruction is done.
            wait(0);
        } catch (exception) {
            fail(exception.message);
        }
    }

    property Component implicitlySizedComponent: Item {
        implicitWidth: 32
        implicitHeight: implicitWidth
        anchors.centerIn: parent

        property alias canvas: canvas

        Canvas {
            id: canvas
            width: Math.max(1, Math.min(parent.width, parent.height))
            height: width

            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                ctx.beginPath();
                ctx.fillRect(0, 0, width, height);
            }
        }
    }

    function test_implicitlySizedParent() {
        var implicitlySizedItem = implicitlySizedComponent.createObject(container);
        verify(implicitlySizedItem);

        var xCenter = implicitlySizedItem.width / 2;
        var yCenter = implicitlySizedItem.height / 2;
        waitForRendering(implicitlySizedItem);
        comparePixel(implicitlySizedItem.canvas.context, xCenter, yCenter, 0, 0, 0, 255);
    }

    Component {
        id: simpleTextureNodeUsageComponent

        Canvas {
            id: canvas
            anchors.fill: parent

            onPaint: {
                var ctx = canvas.getContext("2d");
                ctx.clearRect(0, 0, 200, 200);
            }
        }
    }

    function test_simpleTextureNodeUsage() {
        var canvas = simpleTextureNodeUsageComponent.createObject(container);
        verify(canvas);
        wait(0);
        // Shouldn't crash.
        canvas.requestPaint();
    }
}
}
