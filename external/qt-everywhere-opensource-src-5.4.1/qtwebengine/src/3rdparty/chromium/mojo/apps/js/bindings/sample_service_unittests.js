// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    "console",
    "mojo/apps/js/test/hexdump",
    "gin/test/expect",
    "mojo/public/interfaces/bindings/tests/sample_service.mojom",
    "mojo/public/interfaces/bindings/tests/sample_import.mojom",
    "mojo/public/interfaces/bindings/tests/sample_import2.mojom",
  ], function(console, hexdump, expect, sample, imported, imported2) {

  var global = this;

  // Set this variable to true to print the binary message in hex.
  var dumpMessageAsHex = false;

  function makeFoo() {
    var bar = new sample.Bar();
    bar.alpha = 20;
    bar.beta = 40;
    bar.gamma = 60;
    bar.type = sample.Bar.Type.TYPE_VERTICAL;

    var extra_bars = new Array(3);
    for (var i = 0; i < extra_bars.length; ++i) {
      var base = i * 100;
      var type = i % 2 ?
          sample.Bar.Type.TYPE_VERTICAL : sample.Bar.Type.TYPE_HORIZONTAL;
      extra_bars[i] = new sample.Bar();
      extra_bars[i].alpha = base;
      extra_bars[i].beta = base + 20;
      extra_bars[i].gamma = base + 40;
      extra_bars[i].type = type;
    }

    var data = new Array(10);
    for (var i = 0; i < data.length; ++i) {
      data[i] = data.length - i;
    }

    var source = 0xFFFF;  // Invent a dummy handle.

    var foo = new sample.Foo();
    foo.name = "foopy";
    foo.x = 1;
    foo.y = 2;
    foo.a = false;
    foo.b = true;
    foo.c = false;
    foo.bar = bar;
    foo.extra_bars = extra_bars;
    foo.data = data;
    foo.source = source;
    return foo;
  }

  // Check that the given |Foo| is identical to the one made by |MakeFoo()|.
  function checkFoo(foo) {
    expect(foo.name).toBe("foopy");
    expect(foo.x).toBe(1);
    expect(foo.y).toBe(2);
    expect(foo.a).toBeFalsy();
    expect(foo.b).toBeTruthy();
    expect(foo.c).toBeFalsy();
    expect(foo.bar.alpha).toBe(20);
    expect(foo.bar.beta).toBe(40);
    expect(foo.bar.gamma).toBe(60);
    expect(foo.bar.type).toBe(sample.Bar.Type.TYPE_VERTICAL);

    expect(foo.extra_bars.length).toBe(3);
    for (var i = 0; i < foo.extra_bars.length; ++i) {
      var base = i * 100;
      var type = i % 2 ?
          sample.Bar.Type.TYPE_VERTICAL : sample.Bar.Type.TYPE_HORIZONTAL;
      expect(foo.extra_bars[i].alpha).toBe(base);
      expect(foo.extra_bars[i].beta).toBe(base + 20);
      expect(foo.extra_bars[i].gamma).toBe(base + 40);
      expect(foo.extra_bars[i].type).toBe(type);
    }

    expect(foo.data.length).toBe(10);
    for (var i = 0; i < foo.data.length; ++i)
      expect(foo.data[i]).toBe(foo.data.length - i);

    expect(foo.source).toBe(0xFFFF);
  }

  // Check that values are set to the defaults if we don't override them.
  function checkDefaultValues() {
    var bar = new sample.Bar();
    expect(bar.alpha).toBe(255);
    expect(bar.type).toBe(sample.Bar.Type.TYPE_VERTICAL);

    var foo = new sample.Foo();
    expect(foo.name).toBe("Fooby");
    expect(foo.a).toBeTruthy();
    // TODO(vtl): crbug.com/375845
    // expect(foo.data).toBeNull();

    var defaults = new sample.DefaultsTest();
    expect(defaults.a0).toBe(-12);
    expect(defaults.a1).toBe(sample.kTwelve);
    expect(defaults.a2).toBe(1234);
    expect(defaults.a3).toBe(34567);
    expect(defaults.a4).toBe(123456);
    // TODO(vtl): crbug.com/375522
    // expect(defaults.a5).toBe(3456789012);
    expect(defaults.a6).toBe(111111111111);
    // TODO(vtl): crbug.com/375522 (Also, can we get exact values for large
    // int64/uint64's in JS?)
    // expect(defaults.a7).toBe(9999999999999999999);
    expect(defaults.a8).toBe(0x12345);
    expect(defaults.a9).toBe(-0x12345);
    expect(defaults.a10).toBe(1234);
    expect(defaults.a11).toBe(true);
    expect(defaults.a12).toBe(false);
    expect(defaults.a13).toBe(123.25);
    expect(defaults.a14).toBe(1234567890.123);
    expect(defaults.a15).toBe(1E10);
    expect(defaults.a16).toBe(-1.2E+20);
    expect(defaults.a17).toBe(1.23E-20);
    expect(defaults.a20).toBe(sample.Bar.Type.TYPE_BOTH);
    expect(defaults.a21).toBeNull();
    expect(defaults.a22).toBeTruthy();
    expect(defaults.a22.shape).toBe(imported.Shape.SHAPE_RECTANGLE);
    expect(defaults.a22.color).toBe(imported2.Color.COLOR_BLACK);
    // TODO(vtl): crbug.com/375845
    // expect(defaults.a21).toBeNull();
    // expect(defaults.a22).toBeNull();
  }

  function ServiceImpl() {
  }

  ServiceImpl.prototype = Object.create(sample.ServiceStub.prototype);

  ServiceImpl.prototype.frobinate = function(foo, baz, port) {
    checkFoo(foo);
    expect(baz).toBe(sample.ServiceStub.BazOptions.BAZ_EXTRA);
    expect(port).toBe(10);
    global.result = "PASS";
  };

  function SimpleMessageReceiver() {
  }

  SimpleMessageReceiver.prototype.accept = function(message) {
    if (dumpMessageAsHex) {
      var uint8Array = new Uint8Array(message.buffer.arrayBuffer);
      console.log(hexdump.dumpArray(uint8Array));
    }
    // Imagine some IPC happened here.
    var serviceImpl = new ServiceImpl();
    serviceImpl.accept(message);
  };

  var receiver = new SimpleMessageReceiver();
  var serviceProxy = new sample.ServiceProxy(receiver);

  checkDefaultValues();

  var foo = makeFoo();
  checkFoo(foo);

  var port = 10;
  serviceProxy.frobinate(foo, sample.ServiceProxy.BazOptions.BAZ_EXTRA, port);
});
