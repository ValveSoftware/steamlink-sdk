// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/bindings/codec", [
  "mojo/public/js/bindings/unicode"
], function(unicode) {

  var kErrorUnsigned = "Passing negative value to unsigned";

  // Memory -------------------------------------------------------------------

  var kAlignment = 8;
  var kHighWordMultiplier = 0x100000000;
  var kHostIsLittleEndian = (function () {
    var endianArrayBuffer = new ArrayBuffer(2);
    var endianUint8Array = new Uint8Array(endianArrayBuffer);
    var endianUint16Array = new Uint16Array(endianArrayBuffer);
    endianUint16Array[0] = 1;
    return endianUint8Array[0] == 1;
  })();

  function align(size) {
    return size + (kAlignment - (size % kAlignment)) % kAlignment;
  }

  function getInt64(dataView, byteOffset, value) {
    var lo, hi;
    if (kHostIsLittleEndian) {
      lo = dataView.getUint32(byteOffset, kHostIsLittleEndian);
      hi = dataView.getInt32(byteOffset + 4, kHostIsLittleEndian);
    } else {
      hi = dataView.getInt32(byteOffset, kHostIsLittleEndian);
      lo = dataView.getUint32(byteOffset + 4, kHostIsLittleEndian);
    }
    return lo + hi * kHighWordMultiplier;
  }

  function getUint64(dataView, byteOffset, value) {
    var lo, hi;
    if (kHostIsLittleEndian) {
      lo = dataView.getUint32(byteOffset, kHostIsLittleEndian);
      hi = dataView.getUint32(byteOffset + 4, kHostIsLittleEndian);
    } else {
      hi = dataView.getUint32(byteOffset, kHostIsLittleEndian);
      lo = dataView.getUint32(byteOffset + 4, kHostIsLittleEndian);
    }
    return lo + hi * kHighWordMultiplier;
  }

  function setInt64(dataView, byteOffset, value) {
    var hi = Math.floor(value / kHighWordMultiplier);
    if (kHostIsLittleEndian) {
      dataView.setInt32(byteOffset, value, kHostIsLittleEndian);
      dataView.setInt32(byteOffset + 4, hi, kHostIsLittleEndian);
    } else {
      dataView.setInt32(byteOffset, hi, kHostIsLittleEndian);
      dataView.setInt32(byteOffset + 4, value, kHostIsLittleEndian);
    }
  }

  function setUint64(dataView, byteOffset, value) {
    var hi = (value / kHighWordMultiplier) | 0;
    if (kHostIsLittleEndian) {
      dataView.setInt32(byteOffset, value, kHostIsLittleEndian);
      dataView.setInt32(byteOffset + 4, hi, kHostIsLittleEndian);
    } else {
      dataView.setInt32(byteOffset, hi, kHostIsLittleEndian);
      dataView.setInt32(byteOffset + 4, value, kHostIsLittleEndian);
    }
  }

  function copyArrayBuffer(dstArrayBuffer, srcArrayBuffer) {
    (new Uint8Array(dstArrayBuffer)).set(new Uint8Array(srcArrayBuffer));
  }

  // Buffer -------------------------------------------------------------------

  function Buffer(sizeOrArrayBuffer) {
    if (sizeOrArrayBuffer instanceof ArrayBuffer) {
      this.arrayBuffer = sizeOrArrayBuffer;
    } else {
      this.arrayBuffer = new ArrayBuffer(sizeOrArrayBuffer);
    };

    this.dataView = new DataView(this.arrayBuffer);
    this.next = 0;
  }

  Buffer.prototype.alloc = function(size) {
    var pointer = this.next;
    this.next += size;
    if (this.next > this.arrayBuffer.byteLength) {
      var newSize = (1.5 * (this.arrayBuffer.byteLength + size)) | 0;
      this.grow(newSize);
    }
    return pointer;
  };

  Buffer.prototype.grow = function(size) {
    var newArrayBuffer = new ArrayBuffer(size);
    copyArrayBuffer(newArrayBuffer, this.arrayBuffer);
    this.arrayBuffer = newArrayBuffer;
    this.dataView = new DataView(this.arrayBuffer);
  };

  Buffer.prototype.trim = function() {
    this.arrayBuffer = this.arrayBuffer.slice(0, this.next);
    this.dataView = new DataView(this.arrayBuffer);
  };

  // Constants ----------------------------------------------------------------

  var kArrayHeaderSize = 8;
  var kStructHeaderSize = 8;
  var kMessageHeaderSize = 16;
  var kMessageWithRequestIDHeaderSize = 24;

  // Decoder ------------------------------------------------------------------

  function Decoder(buffer, handles, base) {
    this.buffer = buffer;
    this.handles = handles;
    this.base = base;
    this.next = base;
  }

  Decoder.prototype.skip = function(offset) {
    this.next += offset;
  };

  Decoder.prototype.readInt8 = function() {
    var result = this.buffer.dataView.getInt8(this.next, kHostIsLittleEndian);
    this.next += 1;
    return result;
  };

  Decoder.prototype.readUint8 = function() {
    var result = this.buffer.dataView.getUint8(this.next, kHostIsLittleEndian);
    this.next += 1;
    return result;
  };

  Decoder.prototype.readInt16 = function() {
    var result = this.buffer.dataView.getInt16(this.next, kHostIsLittleEndian);
    this.next += 2;
    return result;
  };

  Decoder.prototype.readUint16 = function() {
    var result = this.buffer.dataView.getUint16(this.next, kHostIsLittleEndian);
    this.next += 2;
    return result;
  };

  Decoder.prototype.readInt32 = function() {
    var result = this.buffer.dataView.getInt32(this.next, kHostIsLittleEndian);
    this.next += 4;
    return result;
  };

  Decoder.prototype.readUint32 = function() {
    var result = this.buffer.dataView.getUint32(this.next, kHostIsLittleEndian);
    this.next += 4;
    return result;
  };

  Decoder.prototype.readInt64 = function() {
    var result = getInt64(this.buffer.dataView, this.next, kHostIsLittleEndian);
    this.next += 8;
    return result;
  };

  Decoder.prototype.readUint64 = function() {
    var result = getUint64(
        this.buffer.dataView, this.next, kHostIsLittleEndian);
    this.next += 8;
    return result;
  };

  Decoder.prototype.readFloat = function() {
    var result = this.buffer.dataView.getFloat32(
        this.next, kHostIsLittleEndian);
    this.next += 4;
    return result;
  };

  Decoder.prototype.readDouble = function() {
    var result = this.buffer.dataView.getFloat64(
        this.next, kHostIsLittleEndian);
    this.next += 8;
    return result;
  };

  Decoder.prototype.decodePointer = function() {
    // TODO(abarth): To correctly decode a pointer, we need to know the real
    // base address of the array buffer.
    var offsetPointer = this.next;
    var offset = this.readUint64();
    if (!offset)
      return 0;
    return offsetPointer + offset;
  };

  Decoder.prototype.decodeAndCreateDecoder = function(pointer) {
    return new Decoder(this.buffer, this.handles, pointer);
  };

  Decoder.prototype.decodeHandle = function() {
    return this.handles[this.readUint32()];
  };

  Decoder.prototype.decodeString = function() {
    var numberOfBytes = this.readUint32();
    var numberOfElements = this.readUint32();
    var base = this.next;
    this.next += numberOfElements;
    return unicode.decodeUtf8String(
        new Uint8Array(this.buffer.arrayBuffer, base, numberOfElements));
  };

  Decoder.prototype.decodeArray = function(cls) {
    var numberOfBytes = this.readUint32();
    var numberOfElements = this.readUint32();
    var val = new Array(numberOfElements);
    for (var i = 0; i < numberOfElements; ++i) {
      val[i] = cls.decode(this);
    }
    return val;
  };

  Decoder.prototype.decodeStruct = function(cls) {
    return cls.decode(this);
  };

  Decoder.prototype.decodeStructPointer = function(cls) {
    var pointer = this.decodePointer();
    if (!pointer) {
      return null;
    }
    return cls.decode(this.decodeAndCreateDecoder(pointer));
  };

  Decoder.prototype.decodeArrayPointer = function(cls) {
    var pointer = this.decodePointer();
    if (!pointer) {
      return null;
    }
    return this.decodeAndCreateDecoder(pointer).decodeArray(cls);
  };

  Decoder.prototype.decodeStringPointer = function() {
    var pointer = this.decodePointer();
    if (!pointer) {
      return null;
    }
    return this.decodeAndCreateDecoder(pointer).decodeString();
  };

  // Encoder ------------------------------------------------------------------

  function Encoder(buffer, handles, base) {
    this.buffer = buffer;
    this.handles = handles;
    this.base = base;
    this.next = base;
  }

  Encoder.prototype.skip = function(offset) {
    this.next += offset;
  };

  Encoder.prototype.writeInt8 = function(val) {
    // NOTE: Endianness doesn't come into play for single bytes.
    this.buffer.dataView.setInt8(this.next, val);
    this.next += 1;
  };

  Encoder.prototype.writeUint8 = function(val) {
    if (val < 0) {
      throw new Error(kErrorUnsigned);
    }
    // NOTE: Endianness doesn't come into play for single bytes.
    this.buffer.dataView.setUint8(this.next, val);
    this.next += 1;
  };

  Encoder.prototype.writeInt16 = function(val) {
    this.buffer.dataView.setInt16(this.next, val, kHostIsLittleEndian);
    this.next += 2;
  };

  Encoder.prototype.writeUint16 = function(val) {
    if (val < 0) {
      throw new Error(kErrorUnsigned);
    }
    this.buffer.dataView.setUint16(this.next, val, kHostIsLittleEndian);
    this.next += 2;
  };

  Encoder.prototype.writeInt32 = function(val) {
    this.buffer.dataView.setInt32(this.next, val, kHostIsLittleEndian);
    this.next += 4;
  };

  Encoder.prototype.writeUint32 = function(val) {
    if (val < 0) {
      throw new Error(kErrorUnsigned);
    }
    this.buffer.dataView.setUint32(this.next, val, kHostIsLittleEndian);
    this.next += 4;
  };

  Encoder.prototype.writeInt64 = function(val) {
    setInt64(this.buffer.dataView, this.next, val);
    this.next += 8;
  };

  Encoder.prototype.writeUint64 = function(val) {
    if (val < 0) {
      throw new Error(kErrorUnsigned);
    }
    setUint64(this.buffer.dataView, this.next, val);
    this.next += 8;
  };

  Encoder.prototype.writeFloat = function(val) {
    this.buffer.dataView.setFloat32(this.next, val, kHostIsLittleEndian);
    this.next += 4;
  };

  Encoder.prototype.writeDouble = function(val) {
    this.buffer.dataView.setFloat64(this.next, val, kHostIsLittleEndian);
    this.next += 8;
  };

  Encoder.prototype.encodePointer = function(pointer) {
    if (!pointer)
      return this.writeUint64(0);
    // TODO(abarth): To correctly encode a pointer, we need to know the real
    // base address of the array buffer.
    var offset = pointer - this.next;
    this.writeUint64(offset);
  };

  Encoder.prototype.createAndEncodeEncoder = function(size) {
    var pointer = this.buffer.alloc(align(size));
    this.encodePointer(pointer);
    return new Encoder(this.buffer, this.handles, pointer);
  };

  Encoder.prototype.encodeHandle = function(handle) {
    this.handles.push(handle);
    this.writeUint32(this.handles.length - 1);
  };

  Encoder.prototype.encodeString = function(val) {
    var base = this.next + kArrayHeaderSize;
    var numberOfElements = unicode.encodeUtf8String(
        val, new Uint8Array(this.buffer.arrayBuffer, base));
    var numberOfBytes = kArrayHeaderSize + numberOfElements;
    this.writeUint32(numberOfBytes);
    this.writeUint32(numberOfElements);
    this.next += numberOfElements;
  };

  Encoder.prototype.encodeArray = function(cls, val) {
    var numberOfElements = val.length;
    var numberOfBytes = kArrayHeaderSize + cls.encodedSize * numberOfElements;
    this.writeUint32(numberOfBytes);
    this.writeUint32(numberOfElements);
    for (var i = 0; i < numberOfElements; ++i) {
      cls.encode(this, val[i]);
    }
  };

  Encoder.prototype.encodeStruct = function(cls, val) {
    return cls.encode(this, val);
  };

  Encoder.prototype.encodeStructPointer = function(cls, val) {
    if (!val) {
      this.encodePointer(val);
      return;
    }
    var encoder = this.createAndEncodeEncoder(cls.encodedSize);
    cls.encode(encoder, val);
  };

  Encoder.prototype.encodeArrayPointer = function(cls, val) {
    if (!val) {
      this.encodePointer(val);
      return;
    }
    var encodedSize = kArrayHeaderSize + cls.encodedSize * val.length;
    var encoder = this.createAndEncodeEncoder(encodedSize);
    encoder.encodeArray(cls, val);
  };

  Encoder.prototype.encodeStringPointer = function(val) {
    if (!val) {
      this.encodePointer(val);
      return;
    }
    var encodedSize = kArrayHeaderSize + unicode.utf8Length(val);
    var encoder = this.createAndEncodeEncoder(encodedSize);
    encoder.encodeString(val);
  };

  // Message ------------------------------------------------------------------

  var kMessageExpectsResponse = 1 << 0;
  var kMessageIsResponse      = 1 << 1;

  // Skip over num_bytes, num_fields, and message_name.
  var kFlagsOffset = 4 + 4 + 4;

  // Skip over num_bytes, num_fields, message_name, and flags.
  var kRequestIDOffset = 4 + 4 + 4 + 4;

  function Message(buffer, handles) {
    this.buffer = buffer;
    this.handles = handles;
  }

  Message.prototype.setRequestID = function(requestID) {
    // TODO(darin): Verify that space was reserved for this field!
    setUint64(this.buffer.dataView, kRequestIDOffset, requestID);
  };

  Message.prototype.getFlags = function() {
    return this.buffer.dataView.getUint32(kFlagsOffset, kHostIsLittleEndian);
  };

  // MessageBuilder -----------------------------------------------------------

  function MessageBuilder(messageName, payloadSize) {
    // Currently, we don't compute the payload size correctly ahead of time.
    // Instead, we resize the buffer at the end.
    var numberOfBytes = kMessageHeaderSize + payloadSize;
    this.buffer = new Buffer(numberOfBytes);
    this.handles = [];
    var encoder = this.createEncoder(kMessageHeaderSize);
    encoder.writeUint32(kMessageHeaderSize);
    encoder.writeUint32(2);  // num_fields.
    encoder.writeUint32(messageName);
    encoder.writeUint32(0);  // flags.
  }

  MessageBuilder.prototype.createEncoder = function(size) {
    var pointer = this.buffer.alloc(size);
    return new Encoder(this.buffer, this.handles, pointer);
  };

  MessageBuilder.prototype.encodeStruct = function(cls, val) {
    cls.encode(this.createEncoder(cls.encodedSize), val);
  };

  MessageBuilder.prototype.finish = function() {
    // TODO(abarth): Rather than resizing the buffer at the end, we could
    // compute the size we need ahead of time, like we do in C++.
    this.buffer.trim();
    var message = new Message(this.buffer, this.handles);
    this.buffer = null;
    this.handles = null;
    this.encoder = null;
    return message;
  };

  // MessageWithRequestIDBuilder -----------------------------------------------

  function MessageWithRequestIDBuilder(messageName, payloadSize, flags,
                                       requestID) {
    // Currently, we don't compute the payload size correctly ahead of time.
    // Instead, we resize the buffer at the end.
    var numberOfBytes = kMessageWithRequestIDHeaderSize + payloadSize;
    this.buffer = new Buffer(numberOfBytes);
    this.handles = [];
    var encoder = this.createEncoder(kMessageWithRequestIDHeaderSize);
    encoder.writeUint32(kMessageWithRequestIDHeaderSize);
    encoder.writeUint32(3);  // num_fields.
    encoder.writeUint32(messageName);
    encoder.writeUint32(flags);
    encoder.writeUint64(requestID);
  }

  MessageWithRequestIDBuilder.prototype =
      Object.create(MessageBuilder.prototype);

  MessageWithRequestIDBuilder.prototype.constructor =
      MessageWithRequestIDBuilder;

  // MessageReader ------------------------------------------------------------

  function MessageReader(message) {
    this.decoder = new Decoder(message.buffer, message.handles, 0);
    var messageHeaderSize = this.decoder.readUint32();
    this.payloadSize =
        message.buffer.arrayBuffer.byteLength - messageHeaderSize;
    var numFields = this.decoder.readUint32();
    this.messageName = this.decoder.readUint32();
    this.flags = this.decoder.readUint32();
    if (numFields >= 3)
      this.requestID = this.decoder.readUint64();
    this.decoder.skip(messageHeaderSize - this.decoder.next);
  }

  MessageReader.prototype.decodeStruct = function(cls) {
    return cls.decode(this.decoder);
  };

  // Built-in types -----------------------------------------------------------

  function Int8() {
  }

  Int8.encodedSize = 1;

  Int8.decode = function(decoder) {
    return decoder.readInt8();
  };

  Int8.encode = function(encoder, val) {
    encoder.writeInt8(val);
  };

  Uint8.encode = function(encoder, val) {
    encoder.writeUint8(val);
  };

  function Uint8() {
  }

  Uint8.encodedSize = 1;

  Uint8.decode = function(decoder) {
    return decoder.readUint8();
  };

  Uint8.encode = function(encoder, val) {
    encoder.writeUint8(val);
  };

  function Int16() {
  }

  Int16.encodedSize = 2;

  Int16.decode = function(decoder) {
    return decoder.readInt16();
  };

  Int16.encode = function(encoder, val) {
    encoder.writeInt16(val);
  };

  function Uint16() {
  }

  Uint16.encodedSize = 2;

  Uint16.decode = function(decoder) {
    return decoder.readUint16();
  };

  Uint16.encode = function(encoder, val) {
    encoder.writeUint16(val);
  };

  function Int32() {
  }

  Int32.encodedSize = 4;

  Int32.decode = function(decoder) {
    return decoder.readInt32();
  };

  Int32.encode = function(encoder, val) {
    encoder.writeInt32(val);
  };

  function Uint32() {
  }

  Uint32.encodedSize = 4;

  Uint32.decode = function(decoder) {
    return decoder.readUint32();
  };

  Uint32.encode = function(encoder, val) {
    encoder.writeUint32(val);
  };

  function Int64() {
  }

  Int64.encodedSize = 8;

  Int64.decode = function(decoder) {
    return decoder.readInt64();
  };

  Int64.encode = function(encoder, val) {
    encoder.writeInt64(val);
  };

  function Uint64() {
  }

  Uint64.encodedSize = 8;

  Uint64.decode = function(decoder) {
    return decoder.readUint64();
  };

  Uint64.encode = function(encoder, val) {
    encoder.writeUint64(val);
  };

  function String() {
  };

  String.encodedSize = 8;

  String.decode = function(decoder) {
    return decoder.decodeStringPointer();
  };

  String.encode = function(encoder, val) {
    encoder.encodeStringPointer(val);
  };


  function Float() {
  }

  Float.encodedSize = 4;

  Float.decode = function(decoder) {
    return decoder.readFloat();
  };

  Float.encode = function(encoder, val) {
    encoder.writeFloat(val);
  };

  function Double() {
  }

  Double.encodedSize = 8;

  Double.decode = function(decoder) {
    return decoder.readDouble();
  };

  Double.encode = function(encoder, val) {
    encoder.writeDouble(val);
  };

  function PointerTo(cls) {
    this.cls = cls;
  }

  PointerTo.prototype.encodedSize = 8;

  PointerTo.prototype.decode = function(decoder) {
    var pointer = decoder.decodePointer();
    if (!pointer) {
      return null;
    }
    return this.cls.decode(decoder.decodeAndCreateDecoder(pointer));
  };

  PointerTo.prototype.encode = function(encoder, val) {
    if (!val) {
      encoder.encodePointer(val);
      return;
    }
    var objectEncoder = encoder.createAndEncodeEncoder(this.cls.encodedSize);
    this.cls.encode(objectEncoder, val);
  };

  function ArrayOf(cls) {
    this.cls = cls;
  }

  ArrayOf.prototype.encodedSize = 8;

  ArrayOf.prototype.decode = function(decoder) {
    return decoder.decodeArrayPointer(this.cls);
  };

  ArrayOf.prototype.encode = function(encoder, val) {
    encoder.encodeArrayPointer(this.cls, val);
  };

  function Handle() {
  }

  Handle.encodedSize = 4;

  Handle.decode = function(decoder) {
    return decoder.decodeHandle();
  };

  Handle.encode = function(encoder, val) {
    encoder.encodeHandle(val);
  };

  var exports = {};
  exports.align = align;
  exports.Buffer = Buffer;
  exports.Message = Message;
  exports.MessageBuilder = MessageBuilder;
  exports.MessageWithRequestIDBuilder = MessageWithRequestIDBuilder;
  exports.MessageReader = MessageReader;
  exports.kArrayHeaderSize = kArrayHeaderSize;
  exports.kStructHeaderSize = kStructHeaderSize;
  exports.kMessageHeaderSize = kMessageHeaderSize;
  exports.kMessageExpectsResponse = kMessageExpectsResponse;
  exports.kMessageIsResponse = kMessageIsResponse;
  exports.Int8 = Int8;
  exports.Uint8 = Uint8;
  exports.Int16 = Int16;
  exports.Uint16 = Uint16;
  exports.Int32 = Int32;
  exports.Uint32 = Uint32;
  exports.Int64 = Int64;
  exports.Uint64 = Uint64;
  exports.Float = Float;
  exports.Double = Double;
  exports.String = String;
  exports.PointerTo = PointerTo;
  exports.ArrayOf = ArrayOf;
  exports.Handle = Handle;
  return exports;
});
