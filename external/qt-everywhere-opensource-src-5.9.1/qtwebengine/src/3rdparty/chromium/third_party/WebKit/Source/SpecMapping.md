# Mapping of spec concepts to code

Blink is Chromium's implementation of the open web platform. This document
attempts to map terms and concepts found in the specification of the open web
platform to classes and files found in Blink's source tree.

[TOC]

## HTML

Concepts found in the [HTML spec](https://html.spec.whatwg.org/).

### [browsing context](https://html.spec.whatwg.org/#browsing-context)

A browsing context corresponds to the
[Frame](https://cs.chromium.org/src/third_party/WebKit/Source/core/frame/Frame.h)
interface where the main implementation is
[LocalFrame](https://cs.chromium.org/src/third_party/WebKit/Source/core/frame/LocalFrame.h).

### [Window object](https://html.spec.whatwg.org/#window)

A Window object corresponds to the
[DOMWindow](https://cs.chromium.org/src/third_party/WebKit/Source/core/frame/DOMWindow.h)
interface where the main implementation is
[LocalDOMWindow](https://cs.chromium.org/src/third_party/WebKit/Source/core/frame/LocalDOMWindow.h).

### [WindowProxy](https://html.spec.whatwg.org/#windowproxy)

The WindowProxy is part of the bindings implemented by a class of the [same
name](https://cs.chromium.org/Source/bindings/core/v8/WindowProxy.h).

### [canvas](https://html.spec.whatwg.org/multipage/scripting.html#the-canvas-element)

An HTML element into which drawing can be performed imperatively via
JavaScript. Multiple
[context types](https://html.spec.whatwg.org/multipage/scripting.html#dom-canvas-getcontext)
are supported for different use cases.

The main element's sources are in
[HTMLCanvasElement](https://cs.chromium.org/chromium/src/third_party/WebKit/Source/core/html/HTMLCanvasElement.h). Contexts
are implemented via modules. The top-level module is
[HTMLCanvasElementModule](https://cs.chromium.org/chromium/src/third_party/WebKit/Source/modules/canvas/HTMLCanvasElementModule.h).

The
[2D canvas context](https://html.spec.whatwg.org/multipage/scripting.html#canvasrenderingcontext2d)
is implemented in
[modules/canvas2d](https://cs.chromium.org/chromium/src/third_party/WebKit/Source/modules/canvas2d/).

The
[WebGL 1.0](https://www.khronos.org/registry/webgl/specs/latest/1.0/)
and
[WebGL 2.0](https://www.khronos.org/registry/webgl/specs/latest/2.0/)
contexts ([Github repo](https://github.com/KhronosGroup/WebGL)) are
implemented in [modules/webgl](https://cs.chromium.org/chromium/src/third_party/WebKit/Source/modules/webgl/).
