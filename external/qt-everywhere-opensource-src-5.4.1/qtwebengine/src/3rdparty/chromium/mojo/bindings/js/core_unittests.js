// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    "gin/test/expect",
    "mojo/public/js/bindings/core",
    "gc",
  ], function(expect, core, gc) {
  runWithMessagePipe(testNop);
  runWithMessagePipe(testReadAndWriteMessage);
  runWithDataPipe(testNop);
  runWithDataPipe(testReadAndWriteDataPipe);
  gc.collectGarbage();  // should not crash
  this.result = "PASS";

  function runWithMessagePipe(test) {
    var pipe = core.createMessagePipe();

    test(pipe);

    expect(core.close(pipe.handle0)).toBe(core.RESULT_OK);
    expect(core.close(pipe.handle1)).toBe(core.RESULT_OK);
  }

  function runWithDataPipe(test) {
    var pipe = core.createDataPipe({
        flags: core.CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,
        elementNumBytes: 1,
        capacityNumBytes: 64
        });
    expect(pipe.result).toBe(core.RESULT_OK);

    test(pipe);

    expect(core.close(pipe.producerHandle)).toBe(core.RESULT_OK);
    expect(core.close(pipe.consumerHandle)).toBe(core.RESULT_OK);
  }

  function testNop(pipe) {
  }

  function testReadAndWriteMessage(pipe) {
    var senderData = new Uint8Array(42);
    for (var i = 0; i < senderData.length; ++i) {
      senderData[i] = i * i;
    }

    var result = core.writeMessage(
      pipe.handle0, senderData, [],
      core.WRITE_MESSAGE_FLAG_NONE);

    expect(result).toBe(core.RESULT_OK);

    var read = core.readMessage(
      pipe.handle1, core.READ_MESSAGE_FLAG_NONE);

    expect(read.result).toBe(core.RESULT_OK);
    expect(read.buffer.byteLength).toBe(42);
    expect(read.handles.length).toBe(0);

    var memory = new Uint8Array(read.buffer);
    for (var i = 0; i < memory.length; ++i)
      expect(memory[i]).toBe((i * i) & 0xFF);
  }

  function testReadAndWriteDataPipe(pipe) {
    var senderData = new Uint8Array(42);
    for (var i = 0; i < senderData.length; ++i) {
      senderData[i] = i * i;
    }

    var write = core.writeData(
      pipe.producerHandle, senderData,
      core.WRITE_DATA_FLAG_ALL_OR_NONE);

    expect(write.result).toBe(core.RESULT_OK);
    expect(write.numBytes).toBe(42);

    var read = core.readData(
      pipe.consumerHandle, core.READ_DATA_FLAG_ALL_OR_NONE);

    expect(read.result).toBe(core.RESULT_OK);
    expect(read.buffer.byteLength).toBe(42);

    var memory = new Uint8Array(read.buffer);
    for (var i = 0; i < memory.length; ++i)
      expect(memory[i]).toBe((i * i) & 0xFF);
  }

});
