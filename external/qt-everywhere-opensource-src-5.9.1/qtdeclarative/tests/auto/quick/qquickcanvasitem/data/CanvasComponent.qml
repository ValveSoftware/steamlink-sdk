import QtQuick 2.0
import QtTest 1.1

Component {
        id:canvas
        Canvas {
            id:c
             antialiasing: false;
             width:100;height:100
             onPaint :{} //this line is needed for some tests (make sure onPaint handler always called
             property alias paintCount:spyPaint.count
             property alias paintedCount:spyPainted.count
             property alias canvasSizeChangedCount:spyCanvasSizeChanged.count
             property alias tileSizeChangedCount:spyTileSizeChanged.count
             property alias renderStrategyChangedCount:spyRenderStrategyChanged.count
             property alias canvasWindowChangedCount:spyCanvasWindowChanged.count
             property alias renderTargetChangedCount:spyRenderTargetChanged.count
             property alias imageLoadedCount:spyImageLoaded.count
             property alias availableChangedCount:spyAvailableChanged.count

             SignalSpy {id: spyPaint;target:c;signalName: "paint"}
             SignalSpy {id: spyPainted;target:c;signalName: "painted"}
             SignalSpy {id: spyCanvasSizeChanged;target:c;signalName: "canvasSizeChanged"}
             SignalSpy {id: spyTileSizeChanged;target:c;signalName: "tileSizeChanged"}
             SignalSpy {id: spyRenderStrategyChanged;target:c;signalName: "renderStrategyChanged"}
             SignalSpy {id: spyCanvasWindowChanged;target:c;signalName: "canvasWindowChanged"}
             SignalSpy {id: spyRenderTargetChanged;target:c;signalName: "renderTargetChanged"}
             SignalSpy {id: spyImageLoaded;target:c;signalName: "imageLoaded"}
             SignalSpy {id: spyAvailableChanged;target:c;signalName: "availableChanged"}
        }
}
