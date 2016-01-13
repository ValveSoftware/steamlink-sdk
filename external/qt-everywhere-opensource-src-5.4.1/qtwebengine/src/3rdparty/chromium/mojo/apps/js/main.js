// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    'console',
    'monotonic_clock',
    'timer',
    'mojo/public/js/bindings/connection',
    'mojo/public/js/bindings/core',
    'mojo/apps/js/bindings/gl',
    'mojo/apps/js/bindings/threading',
    'mojo/services/native_viewport/native_viewport.mojom',
    'mojo/public/interfaces/service_provider/service_provider.mojom',
], function(console,
            monotonicClock,
            timer,
            connection,
            core,
            gljs,
            threading,
            nativeViewport,
            service_provider) {

  const VERTEX_SHADER_SOURCE = [
    'uniform mat4 u_mvpMatrix;',
    'attribute vec4 a_position;',
    'void main()',
    '{',
    '   gl_Position = u_mvpMatrix * a_position;',
    '}'
  ].join('\n');

  const FRAGMENT_SHADER_SOURCE = [
    'precision mediump float;',
    'void main()',
    '{',
    '  gl_FragColor = vec4( 0.0, 1.0, 0.0, 1.0 );',
    '}'
  ].join('\n');

  function ESMatrix() {
    this.m = new Float32Array(16);
  }

  ESMatrix.prototype.getIndex = function(x, y) {
    return x * 4 + y;
  }

  ESMatrix.prototype.set = function(x, y, v) {
    this.m[this.getIndex(x, y)] = v;
  };

  ESMatrix.prototype.get = function(x, y) {
    return this.m[this.getIndex(x, y)];
  };

  ESMatrix.prototype.loadZero = function() {
    for (var i = 0; i < this.m.length; i++) {
      this.m[i] = 0;
    }
  };

  ESMatrix.prototype.loadIdentity = function() {
    this.loadZero();
    for (var i = 0; i < 4; i++) {
      this.set(i, i, 1);
    }
  };

  ESMatrix.prototype.multiply = function(a, b) {
    var result = new ESMatrix();
    for (var i = 0; i < 4; i++) {
      result.set(i, 0,
          (a.get(i, 0) * b.get(0, 0)) +
          (a.get(i, 1) * b.get(1, 0)) +
          (a.get(i, 2) * b.get(2, 0)) +
          (a.get(i, 3) * b.get(3, 0)));

      result.set(i, 1,
          (a.get(i, 0) * b.get(0, 1)) +
          (a.get(i, 1) * b.get(1, 1)) +
          (a.get(i, 2) * b.get(2, 1)) +
          (a.get(i, 3) * b.get(3, 1)));

      result.set(i, 2,
          (a.get(i, 0) * b.get(0, 2)) +
          (a.get(i, 1) * b.get(1, 2)) +
          (a.get(i, 2) * b.get(2, 2)) +
          (a.get(i, 3) * b.get(3, 2)));

      result.set(i, 3,
          (a.get(i, 0) * b.get(0, 3)) +
          (a.get(i, 1) * b.get(1, 3)) +
          (a.get(i, 2) * b.get(2, 3)) +
          (a.get(i, 3) * b.get(3, 3)));
    }
    for (var i = 0; i < result.m.length; i++) {
      this.m[i] = result.m[i];
    }
  };

  ESMatrix.prototype.frustrum = function(left, right, bottom, top, nearZ,
                                         farZ) {
    var deltaX = right - left;
    var deltaY = top - bottom;
    var deltaZ = farZ - nearZ;

    if (nearZ < 0 || farZ < 0 || deltaZ < 0 || deltaY < 0 || deltaX < 0) {
      return;
    }

    var frust = new ESMatrix();
    frust.set(0, 0, 2 * nearZ / deltaX);

    frust.set(1, 1, 2 * nearZ / deltaY);

    frust.set(2, 0, (right + left) / deltaX);
    frust.set(2, 1, (top + bottom) / deltaY);
    frust.set(2, 2, -(nearZ + farZ) / deltaZ);
    frust.set(2, 3, -1);

    frust.set(3, 2, -2 * nearZ * farZ / deltaZ);

    this.multiply(frust, this);
  };

  ESMatrix.prototype.perspective = function(fovY, aspect, nearZ, farZ) {
    var frustrumH = Math.tan(fovY / 360 * Math.PI) * nearZ;
    var frustrumW = frustrumH * aspect;
    this.frustrum(-frustrumW, frustrumW, -frustrumH, frustrumH, nearZ, farZ);
  };

  ESMatrix.prototype.translate = function(tx, ty, tz) {
    this.set(3, 0, this.get(3, 0) + this.get(0, 0) *
             tx + this.get(1, 0) * ty + this.get(2, 0) * tz);
    this.set(3, 1, this.get(3, 1) + this.get(0, 1) *
             tx + this.get(1, 1) * ty + this.get(2, 1) * tz);
    this.set(3, 2, this.get(3, 2) + this.get(0, 2) *
             tx + this.get(1, 2) * ty + this.get(2, 2) * tz);
    this.set(3, 3, this.get(3, 3) + this.get(0, 3) *
             tx + this.get(1, 3) * ty + this.get(2, 3) * tz);
  };

  ESMatrix.prototype.rotate = function(angle, x, y, z) {
    var mag = Math.sqrt(x * x + y * y + z * z);
    var sinAngle = Math.sin(angle * Math.PI / 180);
    var cosAngle = Math.cos(angle * Math.PI / 180);
    if (mag <= 0) {
      return;
    }

    var xx, yy, zz, xy, yz, zx, xs, ys, zs, oneMinusCos;
    var rotation = new ESMatrix();

    x /= mag;
    y /= mag;
    z /= mag;

    xx = x * x;
    yy = y * y;
    zz = z * z;
    xy = x * y;
    yz = y * z;
    zx = z * x;
    xs = x * sinAngle;
    ys = y * sinAngle;
    zs = z * sinAngle;
    oneMinusCos = 1 - cosAngle;

    rotation.set(0, 0, (oneMinusCos * xx) + cosAngle);
    rotation.set(0, 1, (oneMinusCos * xy) - zs);
    rotation.set(0, 2, (oneMinusCos * zx) + ys);
    rotation.set(0, 3, 0);

    rotation.set(1, 0, (oneMinusCos * xy) + zs);
    rotation.set(1, 1, (oneMinusCos * yy) + cosAngle);
    rotation.set(1, 2, (oneMinusCos * yz) - xs);
    rotation.set(1, 3, 0);

    rotation.set(2, 0, (oneMinusCos * zx) - ys);
    rotation.set(2, 1, (oneMinusCos * yz) + xs);
    rotation.set(2, 2, (oneMinusCos * zz) + cosAngle);
    rotation.set(2, 3, 0);

    rotation.set(3, 0, 0);
    rotation.set(3, 1, 0);
    rotation.set(3, 2, 0);
    rotation.set(3, 3, 1);

    this.multiply(rotation, this);
  };

  function loadProgram(gl) {
    var vertexShader = gl.createShader(gl.VERTEX_SHADER);
    gl.shaderSource(vertexShader, VERTEX_SHADER_SOURCE);
    gl.compileShader(vertexShader);

    var fragmentShader = gl.createShader(gl.FRAGMENT_SHADER);
    gl.shaderSource(fragmentShader, FRAGMENT_SHADER_SOURCE);
    gl.compileShader(fragmentShader);

    var program = gl.createProgram();
    gl.attachShader(program, vertexShader);
    gl.attachShader(program, fragmentShader);

    gl.linkProgram(program);
    // TODO(aa): Check for errors using getProgramiv and LINK_STATUS.

    gl.deleteShader(vertexShader);
    gl.deleteShader(fragmentShader);

    return program;
  }

  var vboVertices;
  var vboIndices;
  function generateCube(gl) {
    var numVertices = 24 * 3;
    var numIndices = 12 * 3;

    var cubeVertices = new Float32Array([
      -0.5, -0.5, -0.5,
      -0.5, -0.5,  0.5,
      0.5, -0.5,  0.5,
      0.5, -0.5, -0.5,
      -0.5,  0.5, -0.5,
      -0.5,  0.5,  0.5,
      0.5,  0.5,  0.5,
      0.5,  0.5, -0.5,
      -0.5, -0.5, -0.5,
      -0.5,  0.5, -0.5,
      0.5,  0.5, -0.5,
      0.5, -0.5, -0.5,
      -0.5, -0.5, 0.5,
      -0.5,  0.5, 0.5,
      0.5,  0.5, 0.5,
      0.5, -0.5, 0.5,
      -0.5, -0.5, -0.5,
      -0.5, -0.5,  0.5,
      -0.5,  0.5,  0.5,
      -0.5,  0.5, -0.5,
      0.5, -0.5, -0.5,
      0.5, -0.5,  0.5,
      0.5,  0.5,  0.5,
      0.5,  0.5, -0.5
    ]);

    var cubeIndices = new Uint16Array([
      0, 2, 1,
      0, 3, 2,
      4, 5, 6,
      4, 6, 7,
      8, 9, 10,
      8, 10, 11,
      12, 15, 14,
      12, 14, 13,
      16, 17, 18,
      16, 18, 19,
      20, 23, 22,
      20, 22, 21
    ]);

    // TODO(aa): The C++ program branches here on whether the pointer is
    // non-NULL.
    vboVertices = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, vboVertices);
    gl.bufferData(gl.ARRAY_BUFFER, cubeVertices, gl.STATIC_DRAW);
    gl.bindBuffer(gl.ARRAY_BUFFER, 0);

    vboIndices = gl.createBuffer();
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, vboIndices);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, cubeIndices, gl.STATIC_DRAW);
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, 0);

    return cubeIndices.length;
  }

  function SampleApp(service_provider) {
    this.service_provider_ = service_provider;

    var pipe = new core.createMessagePipe();
    this.service_provider_.connect('mojo:mojo_native_viewport_service',
                                   pipe.handle1);
    new connection.Connection(pipe.handle0, NativeViewportClientImpl,
                              nativeViewport.NativeViewportProxy);
  }
  // TODO(aa): It is a bummer to need this stub object in JavaScript. We should
  // have a 'client' object that contains both the sending and receiving bits of
  // the client side of the interface. Since JS is loosely typed, we do not need
  // a separate base class to inherit from to receive callbacks.
  SampleApp.prototype =
      Object.create(service_provider.ServiceProviderStub.prototype);


  function NativeViewportClientImpl(remote) {
    this.remote_ = remote;

    var pipe = core.createMessagePipe();

    var rect = new nativeViewport.Rect;
    rect.position = new nativeViewport.Point;
    rect.size = new nativeViewport.Size;
    rect.size.width = 800;
    rect.size.height = 600;
    this.remote_.create(rect);
    this.remote_.show();
    this.remote_.createGLES2Context(pipe.handle1);
    this.gles2_ = new GLES2ClientImpl(pipe.handle0);
  }
  NativeViewportClientImpl.prototype =
      Object.create(nativeViewport.NativeViewportClientStub.prototype);

  NativeViewportClientImpl.prototype.onCreated = function() {
    console.log('NativeViewportClientImpl.prototype.OnCreated');
  };

  NativeViewportClientImpl.prototype.onBoundsChanged = function(bounds) {
    console.log('NativeViewportClientImpl.prototype.OnBoundsChanged');
    this.gles2_.setDimensions(bounds.size);
  }

  function GLES2ClientImpl(remotePipe) {
    this.gl_ = new gljs.Context(remotePipe, this.contextLost.bind(this));
    this.lastTime_ = monotonicClock.seconds();
    this.angle_ = 45;

    this.program_ = loadProgram(this.gl_);
    this.positionLocation_ =
        this.gl_.getAttribLocation(this.program_, 'a_position');
    this.mvpLocation_ =
        this.gl_.getUniformLocation(this.program_, 'u_mvpMatrix');
    this.numIndices_ = generateCube(this.gl_);
    this.mvpMatrix_ = new ESMatrix();
    this.mvpMatrix_.loadIdentity();

    this.gl_.clearColor(0, 0, 0, 0);
  }

  GLES2ClientImpl.prototype.setDimensions = function(size) {
    this.width_ = size.width;
    this.height_ = size.height;
    this.timer_ = timer.createRepeating(16, this.handleTimer.bind(this));
  }

  GLES2ClientImpl.prototype.drawCube = function() {
    this.gl_.viewport(0, 0, this.width_, this.height_);
    this.gl_.clear(this.gl_.COLOR_BUFFER_BIT);
    this.gl_.useProgram(this.program_);
    this.gl_.bindBuffer(this.gl_.ARRAY_BUFFER, vboVertices);
    this.gl_.bindBuffer(this.gl_.ELEMENT_ARRAY_BUFFER, vboIndices);
    this.gl_.vertexAttribPointer(this.positionLocation_, 3, this.gl_.FLOAT,
                                 false, 12, 0);
    this.gl_.enableVertexAttribArray(this.positionLocation_);
    this.gl_.uniformMatrix4fv(this.mvpLocation_, false, this.mvpMatrix_.m);
    this.gl_.drawElements(this.gl_.TRIANGLES, this.numIndices_,
                          this.gl_.UNSIGNED_SHORT, 0);
    this.gl_.swapBuffers();
  };

  GLES2ClientImpl.prototype.handleTimer = function() {
    var now = monotonicClock.seconds();
    var secondsDelta = now - this.lastTime_;
    this.lastTime_ = now;

    this.angle_ += this.getRotationForTimeDelta(secondsDelta);
    this.angle_ = this.angle_ % 360;

    var aspect = this.width_ / this.height_;

    var perspective = new ESMatrix();
    perspective.loadIdentity();
    perspective.perspective(60, aspect, 1, 20);

    var modelView = new ESMatrix();
    modelView.loadIdentity();
    modelView.translate(0, 0, -2);
    modelView.rotate(this.angle_, 1, 0, 1);

    this.mvpMatrix_.multiply(modelView, perspective);

    this.drawCube();
  };

  GLES2ClientImpl.prototype.getRotationForTimeDelta = function(secondsDelta) {
    return secondsDelta * 40;
  };

  GLES2ClientImpl.prototype.contextLost = function() {
    console.log('GLES2ClientImpl.prototype.contextLost');
  };


  return function(handle) {
    new connection.Connection(
        handle, SampleApp, service_provider.ServiceProviderProxy);
  };
});
