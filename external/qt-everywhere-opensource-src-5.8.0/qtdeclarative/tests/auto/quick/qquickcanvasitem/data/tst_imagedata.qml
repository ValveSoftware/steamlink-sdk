import QtQuick 2.0
import QtTest 1.1

CanvasTestCase {
    id:testCase
    name: "imagedata"
    function init_data() { return testData("2d"); }
    function test_rounding(row) {
        var canvas = createCanvasObject(row);
        var ctx = canvas.getContext('2d');
        var size = 17
        ctx.reset();
        ctx.fillStyle = Qt.rgba(0.7, 0.8, 0.9, 1.0);
        ctx.fillRect(0, 0, size, size);

        var center = size / 2;
        var imageData = ctx.getImageData(center, center, center, center);
        comparePixel(ctx, center, center, imageData.data[0], imageData.data[1], imageData.data[2], imageData.data[3]);

        canvas.destroy();
    }
}
