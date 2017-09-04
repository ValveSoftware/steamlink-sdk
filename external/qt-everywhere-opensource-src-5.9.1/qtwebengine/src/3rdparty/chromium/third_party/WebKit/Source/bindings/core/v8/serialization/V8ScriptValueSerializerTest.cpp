// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/serialization/V8ScriptValueSerializer.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8Blob.h"
#include "bindings/core/v8/V8CompositorProxy.h"
#include "bindings/core/v8/V8DOMException.h"
#include "bindings/core/v8/V8File.h"
#include "bindings/core/v8/V8FileList.h"
#include "bindings/core/v8/V8ImageBitmap.h"
#include "bindings/core/v8/V8ImageData.h"
#include "bindings/core/v8/V8MessagePort.h"
#include "bindings/core/v8/V8OffscreenCanvas.h"
#include "bindings/core/v8/V8StringResource.h"
#include "bindings/core/v8/serialization/V8ScriptValueDeserializer.h"
#include "core/dom/CompositorProxy.h"
#include "core/dom/MessagePort.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/File.h"
#include "core/fileapi/FileList.h"
#include "core/frame/LocalFrame.h"
#include "core/html/ImageData.h"
#include "core/offscreencanvas/OffscreenCanvas.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/CompositorMutableProperties.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "public/platform/WebBlobInfo.h"
#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/WebMessagePortChannelClient.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "wtf/CurrentTime.h"
#include "wtf/DateMath.h"

namespace blink {
namespace {

class ScopedEnableV8BasedStructuredClone {
 public:
  ScopedEnableV8BasedStructuredClone()
      : m_wasEnabled(RuntimeEnabledFeatures::v8BasedStructuredCloneEnabled()) {
    RuntimeEnabledFeatures::setV8BasedStructuredCloneEnabled(true);
  }
  ~ScopedEnableV8BasedStructuredClone() {
    RuntimeEnabledFeatures::setV8BasedStructuredCloneEnabled(m_wasEnabled);
  }

 private:
  bool m_wasEnabled;
};

RefPtr<SerializedScriptValue> serializedValue(const Vector<uint8_t>& bytes) {
  // TODO(jbroman): Fix this once SerializedScriptValue can take bytes without
  // endianness swapping.
  DCHECK_EQ(bytes.size() % 2, 0u);
  return SerializedScriptValue::create(
      String(reinterpret_cast<const UChar*>(&bytes[0]), bytes.size() / 2));
}

v8::Local<v8::Value> roundTrip(v8::Local<v8::Value> value,
                               V8TestingScope& scope,
                               ExceptionState* overrideExceptionState = nullptr,
                               Transferables* transferables = nullptr,
                               WebBlobInfoArray* blobInfo = nullptr) {
  RefPtr<ScriptState> scriptState = scope.getScriptState();
  ExceptionState& exceptionState = overrideExceptionState
                                       ? *overrideExceptionState
                                       : scope.getExceptionState();

  // Extract message ports and disentangle them.
  std::unique_ptr<MessagePortChannelArray> channels;
  if (transferables) {
    channels = MessagePort::disentanglePorts(scope.getExecutionContext(),
                                             transferables->messagePorts,
                                             exceptionState);
    if (exceptionState.hadException())
      return v8::Local<v8::Value>();
  }

  V8ScriptValueSerializer serializer(scriptState);
  serializer.setBlobInfoArray(blobInfo);
  RefPtr<SerializedScriptValue> serializedScriptValue =
      serializer.serialize(value, transferables, exceptionState);
  DCHECK_EQ(!serializedScriptValue, exceptionState.hadException());
  if (!serializedScriptValue)
    return v8::Local<v8::Value>();

  // If there are message ports, make new ones and entangle them.
  MessagePortArray* transferredMessagePorts = MessagePort::entanglePorts(
      *scope.getExecutionContext(), std::move(channels));

  V8ScriptValueDeserializer deserializer(scriptState, serializedScriptValue);
  deserializer.setTransferredMessagePorts(transferredMessagePorts);
  deserializer.setBlobInfoArray(blobInfo);
  return deserializer.deserialize();
}

v8::Local<v8::Value> eval(const String& source, V8TestingScope& scope) {
  return scope.frame().script().executeScriptInMainWorldAndReturnValue(source);
}

String toJSON(v8::Local<v8::Object> object, const V8TestingScope& scope) {
  return v8StringToWebCoreString<String>(
      v8::JSON::Stringify(scope.context(), object).ToLocalChecked(),
      DoNotExternalize);
}

// Checks for a DOM exception, including a rethrown one.
::testing::AssertionResult hadDOMException(const StringView& name,
                                           ScriptState* scriptState,
                                           ExceptionState& exceptionState) {
  if (!exceptionState.hadException())
    return ::testing::AssertionFailure() << "no exception thrown";
  DOMException* domException = V8DOMException::toImplWithTypeCheck(
      scriptState->isolate(), exceptionState.getException());
  if (!domException)
    return ::testing::AssertionFailure()
           << "exception thrown was not a DOMException";
  if (domException->name() != name)
    return ::testing::AssertionFailure() << "was " << domException->name();
  return ::testing::AssertionSuccess();
}

TEST(V8ScriptValueSerializerTest, RoundTripJSONLikeValue) {
  // Ensure that simple JavaScript objects work.
  // There are more exhaustive tests of JavaScript objects in V8.
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  v8::Local<v8::Value> object = eval("({ foo: [1, 2, 3], bar: 'baz' })", scope);
  DCHECK(object->IsObject());
  v8::Local<v8::Value> result = roundTrip(object, scope);
  ASSERT_TRUE(result->IsObject());
  EXPECT_NE(object, result);
  EXPECT_EQ(toJSON(object.As<v8::Object>(), scope),
            toJSON(result.As<v8::Object>(), scope));
}

TEST(V8ScriptValueSerializerTest, ThrowsDataCloneError) {
  // Ensure that a proper DataCloneError DOMException is thrown when issues
  // are encountered in V8 (for example, cloning a symbol). It should be an
  // instance of DOMException, and it should have a proper descriptive
  // message.
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();
  ExceptionState exceptionState(scope.isolate(),
                                ExceptionState::ExecutionContext, "Window",
                                "postMessage");
  v8::Local<v8::Value> symbol = eval("Symbol()", scope);
  DCHECK(symbol->IsSymbol());
  ASSERT_FALSE(V8ScriptValueSerializer(scriptState)
                   .serialize(symbol, nullptr, exceptionState));
  ASSERT_TRUE(hadDOMException("DataCloneError", scriptState, exceptionState));
  DOMException* domException =
      V8DOMException::toImpl(exceptionState.getException().As<v8::Object>());
  EXPECT_TRUE(domException->toString().contains("postMessage"));
}

TEST(V8ScriptValueSerializerTest, RethrowsScriptError) {
  // Ensure that other exceptions, like those thrown by script, are properly
  // rethrown.
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();
  ExceptionState exceptionState(scope.isolate(),
                                ExceptionState::ExecutionContext, "Window",
                                "postMessage");
  v8::Local<v8::Value> exception = eval("myException=new Error()", scope);
  v8::Local<v8::Value> object =
      eval("({ get a() { throw myException; }})", scope);
  DCHECK(object->IsObject());
  ASSERT_FALSE(V8ScriptValueSerializer(scriptState)
                   .serialize(object, nullptr, exceptionState));
  ASSERT_TRUE(exceptionState.hadException());
  EXPECT_EQ(exception, exceptionState.getException());
}

TEST(V8ScriptValueSerializerTest, DeserializationErrorReturnsNull) {
  // If there's a problem during deserialization, it results in null, but no
  // exception.
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();
  RefPtr<SerializedScriptValue> invalid =
      SerializedScriptValue::create("invalid data");
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scriptState, invalid).deserialize();
  EXPECT_TRUE(result->IsNull());
  EXPECT_FALSE(scope.getExceptionState().hadException());
}

TEST(V8ScriptValueSerializerTest, NeuteringHappensAfterSerialization) {
  // This object will throw an exception before the [[Transfer]] step.
  // As a result, the ArrayBuffer will not be transferred.
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ExceptionState exceptionState(scope.isolate(),
                                ExceptionState::ExecutionContext, "Window",
                                "postMessage");

  DOMArrayBuffer* arrayBuffer = DOMArrayBuffer::create(1, 1);
  ASSERT_FALSE(arrayBuffer->isNeutered());
  v8::Local<v8::Value> object = eval("({ get a() { throw 'party'; }})", scope);
  Transferables transferables;
  transferables.arrayBuffers.append(arrayBuffer);

  roundTrip(object, scope, &exceptionState, &transferables);
  ASSERT_TRUE(exceptionState.hadException());
  EXPECT_FALSE(hadDOMException("DataCloneError", scope.getScriptState(),
                               exceptionState));
  EXPECT_FALSE(arrayBuffer->isNeutered());
}

TEST(V8ScriptValueSerializerTest, RoundTripImageData) {
  // ImageData objects should serialize and deserialize correctly.
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ImageData* imageData = ImageData::create(2, 1, ASSERT_NO_EXCEPTION);
  imageData->data()->data()[0] = 200;
  v8::Local<v8::Value> wrapper =
      toV8(imageData, scope.context()->Global(), scope.isolate());
  v8::Local<v8::Value> result = roundTrip(wrapper, scope);
  ASSERT_TRUE(V8ImageData::hasInstance(result, scope.isolate()));
  ImageData* newImageData = V8ImageData::toImpl(result.As<v8::Object>());
  EXPECT_NE(imageData, newImageData);
  EXPECT_EQ(imageData->size(), newImageData->size());
  EXPECT_EQ(imageData->data()->length(), newImageData->data()->length());
  EXPECT_EQ(200, newImageData->data()->data()[0]);
}

TEST(V8ScriptValueSerializerTest, DecodeImageData) {
  // Backward compatibility with existing serialized ImageData objects must be
  // maintained. Add more cases if the format changes; don't remove tests for
  // old versions.
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();
  RefPtr<SerializedScriptValue> input =
      serializedValue({0xff, 0x09, 0x3f, 0x00, 0x23, 0x02, 0x01, 0x08, 0xc8,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scriptState, input).deserialize();
  ASSERT_TRUE(V8ImageData::hasInstance(result, scope.isolate()));
  ImageData* newImageData = V8ImageData::toImpl(result.As<v8::Object>());
  EXPECT_EQ(IntSize(2, 1), newImageData->size());
  EXPECT_EQ(8u, newImageData->data()->length());
  EXPECT_EQ(200, newImageData->data()->data()[0]);
}

class WebMessagePortChannelImpl final : public WebMessagePortChannel {
 public:
  // WebMessagePortChannel
  void setClient(WebMessagePortChannelClient* client) override {}
  void destroy() override { delete this; }
  void postMessage(const WebString&, WebMessagePortChannelArray*) {
    NOTIMPLEMENTED();
  }
  bool tryGetMessage(WebString*, WebMessagePortChannelArray&) { return false; }
};

MessagePort* makeMessagePort(
    ExecutionContext* executionContext,
    WebMessagePortChannel** unownedChannelOut = nullptr) {
  auto* unownedChannel = new WebMessagePortChannelImpl();
  MessagePort* port = MessagePort::create(*executionContext);
  port->entangle(WebMessagePortChannelUniquePtr(unownedChannel));
  EXPECT_TRUE(port->isEntangled());
  EXPECT_EQ(unownedChannel, port->entangledChannelForTesting());
  if (unownedChannelOut)
    *unownedChannelOut = unownedChannel;
  return port;
}

TEST(V8ScriptValueSerializerTest, RoundTripMessagePort) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;

  WebMessagePortChannel* unownedChannel;
  MessagePort* port =
      makeMessagePort(scope.getExecutionContext(), &unownedChannel);
  v8::Local<v8::Value> wrapper = toV8(port, scope.getScriptState());
  Transferables transferables;
  transferables.messagePorts.append(port);

  v8::Local<v8::Value> result =
      roundTrip(wrapper, scope, nullptr, &transferables);
  ASSERT_TRUE(V8MessagePort::hasInstance(result, scope.isolate()));
  MessagePort* newPort = V8MessagePort::toImpl(result.As<v8::Object>());
  EXPECT_FALSE(port->isEntangled());
  EXPECT_TRUE(newPort->isEntangled());
  EXPECT_EQ(unownedChannel, newPort->entangledChannelForTesting());
}

TEST(V8ScriptValueSerializerTest, NeuteredMessagePortThrowsDataCloneError) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ExceptionState exceptionState(scope.isolate(),
                                ExceptionState::ExecutionContext, "Window",
                                "postMessage");

  MessagePort* port = MessagePort::create(*scope.getExecutionContext());
  EXPECT_TRUE(port->isNeutered());
  v8::Local<v8::Value> wrapper = toV8(port, scope.getScriptState());
  Transferables transferables;
  transferables.messagePorts.append(port);

  roundTrip(wrapper, scope, &exceptionState, &transferables);
  ASSERT_TRUE(hadDOMException("DataCloneError", scope.getScriptState(),
                              exceptionState));
}

TEST(V8ScriptValueSerializerTest,
     UntransferredMessagePortThrowsDataCloneError) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ExceptionState exceptionState(scope.isolate(),
                                ExceptionState::ExecutionContext, "Window",
                                "postMessage");

  WebMessagePortChannel* unownedChannel;
  MessagePort* port =
      makeMessagePort(scope.getExecutionContext(), &unownedChannel);
  v8::Local<v8::Value> wrapper = toV8(port, scope.getScriptState());
  Transferables transferables;

  roundTrip(wrapper, scope, &exceptionState, &transferables);
  ASSERT_TRUE(hadDOMException("DataCloneError", scope.getScriptState(),
                              exceptionState));
}

TEST(V8ScriptValueSerializerTest, OutOfRangeMessagePortIndex) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();
  RefPtr<SerializedScriptValue> input =
      serializedValue({0xff, 0x09, 0x3f, 0x00, 0x4d, 0x01});
  MessagePort* port1 = makeMessagePort(scope.getExecutionContext());
  MessagePort* port2 = makeMessagePort(scope.getExecutionContext());
  {
    V8ScriptValueDeserializer deserializer(scriptState, input);
    ASSERT_TRUE(deserializer.deserialize()->IsNull());
  }
  {
    V8ScriptValueDeserializer deserializer(scriptState, input);
    deserializer.setTransferredMessagePorts(new MessagePortArray);
    ASSERT_TRUE(deserializer.deserialize()->IsNull());
  }
  {
    MessagePortArray* ports = new MessagePortArray;
    ports->append(port1);
    V8ScriptValueDeserializer deserializer(scriptState, input);
    deserializer.setTransferredMessagePorts(ports);
    ASSERT_TRUE(deserializer.deserialize()->IsNull());
  }
  {
    MessagePortArray* ports = new MessagePortArray;
    ports->append(port1);
    ports->append(port2);
    V8ScriptValueDeserializer deserializer(scriptState, input);
    deserializer.setTransferredMessagePorts(ports);
    v8::Local<v8::Value> result = deserializer.deserialize();
    ASSERT_TRUE(V8MessagePort::hasInstance(result, scope.isolate()));
    EXPECT_EQ(port2, V8MessagePort::toImpl(result.As<v8::Object>()));
  }
}

// Decode tests for backward compatibility are not required for message ports
// because they cannot be persisted to disk.

// A more exhaustive set of ImageBitmap cases are covered by LayoutTests.
TEST(V8ScriptValueSerializerTest, RoundTripImageBitmap) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;

  // Make a 10x7 red ImageBitmap.
  sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(10, 7);
  surface->getCanvas()->clear(SK_ColorRED);
  ImageBitmap* imageBitmap = ImageBitmap::create(
      StaticBitmapImage::create(surface->makeImageSnapshot()));

  // Serialize and deserialize it.
  v8::Local<v8::Value> wrapper = toV8(imageBitmap, scope.getScriptState());
  v8::Local<v8::Value> result = roundTrip(wrapper, scope);
  ASSERT_TRUE(V8ImageBitmap::hasInstance(result, scope.isolate()));
  ImageBitmap* newImageBitmap = V8ImageBitmap::toImpl(result.As<v8::Object>());
  ASSERT_EQ(IntSize(10, 7), newImageBitmap->size());

  // Check that the pixel at (3, 3) is red.
  uint8_t pixel[4] = {};
  ASSERT_TRUE(newImageBitmap->bitmapImage()->imageForCurrentFrame()->readPixels(
      SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType, kPremul_SkAlphaType),
      &pixel, 4, 3, 3));
  ASSERT_THAT(pixel, ::testing::ElementsAre(255, 0, 0, 255));
}

TEST(V8ScriptValueSerializerTest, DecodeImageBitmap) {
  // Backward compatibility with existing serialized ImageBitmap objects must be
  // maintained. Add more cases if the format changes; don't remove tests for
  // old versions.
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();

// This is checked by platform instead of by SK_PMCOLOR_BYTE_ORDER because
// this test intends to ensure that a platform can decode images it has
// previously written. At format version 9, Android writes RGBA and every
// other platform writes BGRA.
#if OS(ANDROID)
  RefPtr<SerializedScriptValue> input =
      serializedValue({0xff, 0x09, 0x3f, 0x00, 0x67, 0x01, 0x01, 0x02, 0x01,
                       0x08, 0xff, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff});
#else
  RefPtr<SerializedScriptValue> input =
      serializedValue({0xff, 0x09, 0x3f, 0x00, 0x67, 0x01, 0x01, 0x02, 0x01,
                       0x08, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff});
#endif

  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scriptState, input).deserialize();
  ASSERT_TRUE(V8ImageBitmap::hasInstance(result, scope.isolate()));
  ImageBitmap* newImageBitmap = V8ImageBitmap::toImpl(result.As<v8::Object>());
  ASSERT_EQ(IntSize(2, 1), newImageBitmap->size());

  // Check that the pixels are opaque red and green, respectively.
  uint8_t pixels[8] = {};
  ASSERT_TRUE(newImageBitmap->bitmapImage()->imageForCurrentFrame()->readPixels(
      SkImageInfo::Make(2, 1, kRGBA_8888_SkColorType, kPremul_SkAlphaType),
      &pixels, 8, 0, 0));
  ASSERT_THAT(pixels, ::testing::ElementsAre(255, 0, 0, 255, 0, 255, 0, 255));
}

TEST(V8ScriptValueSerializerTest, InvalidImageBitmapDecode) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  ScriptState* scriptState = scope.getScriptState();
  {
    // Too many bytes declared in pixel data.
    RefPtr<SerializedScriptValue> input = serializedValue(
        {0xff, 0x09, 0x3f, 0x00, 0x67, 0x01, 0x01, 0x02, 0x01, 0x09,
         0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00});
    EXPECT_TRUE(
        V8ScriptValueDeserializer(scriptState, input).deserialize()->IsNull());
  }
  {
    // Too few bytes declared in pixel data.
    RefPtr<SerializedScriptValue> input =
        serializedValue({0xff, 0x09, 0x3f, 0x00, 0x67, 0x01, 0x01, 0x02, 0x01,
                         0x07, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff});
    EXPECT_TRUE(
        V8ScriptValueDeserializer(scriptState, input).deserialize()->IsNull());
  }
  {
    // Nonsense for origin clean data.
    RefPtr<SerializedScriptValue> input =
        serializedValue({0xff, 0x09, 0x3f, 0x00, 0x67, 0x02, 0x01, 0x02, 0x01,
                         0x08, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff});
    EXPECT_TRUE(
        V8ScriptValueDeserializer(scriptState, input).deserialize()->IsNull());
  }
  {
    // Nonsense for premultiplied bit.
    RefPtr<SerializedScriptValue> input =
        serializedValue({0xff, 0x09, 0x3f, 0x00, 0x67, 0x01, 0x02, 0x02, 0x01,
                         0x08, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff});
    EXPECT_TRUE(
        V8ScriptValueDeserializer(scriptState, input).deserialize()->IsNull());
  }
}

TEST(V8ScriptValueSerializerTest, TransferImageBitmap) {
  // More thorough tests exist in LayoutTests/.
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;

  sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(10, 7);
  surface->getCanvas()->clear(SK_ColorRED);
  sk_sp<SkImage> image = surface->makeImageSnapshot();
  ImageBitmap* imageBitmap =
      ImageBitmap::create(StaticBitmapImage::create(image));

  v8::Local<v8::Value> wrapper = toV8(imageBitmap, scope.getScriptState());
  Transferables transferables;
  transferables.imageBitmaps.append(imageBitmap);
  v8::Local<v8::Value> result =
      roundTrip(wrapper, scope, nullptr, &transferables);
  ASSERT_TRUE(V8ImageBitmap::hasInstance(result, scope.isolate()));
  ImageBitmap* newImageBitmap = V8ImageBitmap::toImpl(result.As<v8::Object>());
  ASSERT_EQ(IntSize(10, 7), newImageBitmap->size());

  // Check that the pixel at (3, 3) is red.
  uint8_t pixel[4] = {};
  sk_sp<SkImage> newImage =
      newImageBitmap->bitmapImage()->imageForCurrentFrame();
  ASSERT_TRUE(newImage->readPixels(
      SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType, kPremul_SkAlphaType),
      &pixel, 4, 3, 3));
  ASSERT_THAT(pixel, ::testing::ElementsAre(255, 0, 0, 255));

  // Check also that the underlying image contents were transferred.
  EXPECT_EQ(image, newImage);
  EXPECT_TRUE(imageBitmap->isNeutered());
}

TEST(V8ScriptValueSerializerTest, TransferOffscreenCanvas) {
  // More exhaustive tests in LayoutTests/. This is a sanity check.
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  OffscreenCanvas* canvas = OffscreenCanvas::create(10, 7);
  canvas->setPlaceholderCanvasId(519);
  v8::Local<v8::Value> wrapper = toV8(canvas, scope.getScriptState());
  Transferables transferables;
  transferables.offscreenCanvases.append(canvas);
  v8::Local<v8::Value> result =
      roundTrip(wrapper, scope, nullptr, &transferables);
  ASSERT_TRUE(V8OffscreenCanvas::hasInstance(result, scope.isolate()));
  OffscreenCanvas* newCanvas =
      V8OffscreenCanvas::toImpl(result.As<v8::Object>());
  EXPECT_EQ(IntSize(10, 7), newCanvas->size());
  EXPECT_EQ(519, newCanvas->placeholderCanvasId());
  EXPECT_TRUE(canvas->isNeutered());
  EXPECT_FALSE(newCanvas->isNeutered());
}

TEST(V8ScriptValueSerializerTest, RoundTripBlob) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  const char kHelloWorld[] = "Hello world!";
  Blob* blob =
      Blob::create(reinterpret_cast<const unsigned char*>(&kHelloWorld),
                   sizeof(kHelloWorld), "text/plain");
  String uuid = blob->uuid();
  EXPECT_FALSE(uuid.isEmpty());
  v8::Local<v8::Value> wrapper = toV8(blob, scope.getScriptState());
  v8::Local<v8::Value> result = roundTrip(wrapper, scope);
  ASSERT_TRUE(V8Blob::hasInstance(result, scope.isolate()));
  Blob* newBlob = V8Blob::toImpl(result.As<v8::Object>());
  EXPECT_EQ("text/plain", newBlob->type());
  EXPECT_EQ(sizeof(kHelloWorld), newBlob->size());
  EXPECT_EQ(uuid, newBlob->uuid());
}

TEST(V8ScriptValueSerializerTest, DecodeBlob) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input = serializedValue(
      {0xff, 0x09, 0x3f, 0x00, 0x62, 0x24, 0x64, 0x38, 0x37, 0x35, 0x64,
       0x66, 0x63, 0x32, 0x2d, 0x34, 0x35, 0x30, 0x35, 0x2d, 0x34, 0x36,
       0x31, 0x62, 0x2d, 0x39, 0x38, 0x66, 0x65, 0x2d, 0x30, 0x63, 0x66,
       0x36, 0x63, 0x63, 0x35, 0x65, 0x61, 0x66, 0x34, 0x34, 0x0a, 0x74,
       0x65, 0x78, 0x74, 0x2f, 0x70, 0x6c, 0x61, 0x69, 0x6e, 0x0c});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.getScriptState(), input).deserialize();
  ASSERT_TRUE(V8Blob::hasInstance(result, scope.isolate()));
  Blob* newBlob = V8Blob::toImpl(result.As<v8::Object>());
  EXPECT_EQ("d875dfc2-4505-461b-98fe-0cf6cc5eaf44", newBlob->uuid());
  EXPECT_EQ("text/plain", newBlob->type());
  EXPECT_EQ(12u, newBlob->size());
}

TEST(V8ScriptValueSerializerTest, RoundTripBlobIndex) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  const char kHelloWorld[] = "Hello world!";
  Blob* blob =
      Blob::create(reinterpret_cast<const unsigned char*>(&kHelloWorld),
                   sizeof(kHelloWorld), "text/plain");
  String uuid = blob->uuid();
  EXPECT_FALSE(uuid.isEmpty());
  v8::Local<v8::Value> wrapper = toV8(blob, scope.getScriptState());
  WebBlobInfoArray blobInfoArray;
  v8::Local<v8::Value> result =
      roundTrip(wrapper, scope, nullptr, nullptr, &blobInfoArray);

  // As before, the resulting blob should be correct.
  ASSERT_TRUE(V8Blob::hasInstance(result, scope.isolate()));
  Blob* newBlob = V8Blob::toImpl(result.As<v8::Object>());
  EXPECT_EQ("text/plain", newBlob->type());
  EXPECT_EQ(sizeof(kHelloWorld), newBlob->size());
  EXPECT_EQ(uuid, newBlob->uuid());

  // The blob info array should also contain the blob details since it was
  // serialized by index into this array.
  ASSERT_EQ(1u, blobInfoArray.size());
  const WebBlobInfo& info = blobInfoArray[0];
  EXPECT_FALSE(info.isFile());
  EXPECT_EQ(uuid, String(info.uuid()));
  EXPECT_EQ("text/plain", info.type());
  EXPECT_EQ(sizeof(kHelloWorld), static_cast<size_t>(info.size()));
}

TEST(V8ScriptValueSerializerTest, DecodeBlobIndex) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input =
      serializedValue({0xff, 0x09, 0x3f, 0x00, 0x69, 0x00});
  WebBlobInfoArray blobInfoArray;
  blobInfoArray.emplace_back("d875dfc2-4505-461b-98fe-0cf6cc5eaf44",
                             "text/plain", 12);
  V8ScriptValueDeserializer deserializer(scope.getScriptState(), input);
  deserializer.setBlobInfoArray(&blobInfoArray);
  v8::Local<v8::Value> result = deserializer.deserialize();
  ASSERT_TRUE(V8Blob::hasInstance(result, scope.isolate()));
  Blob* newBlob = V8Blob::toImpl(result.As<v8::Object>());
  EXPECT_EQ("d875dfc2-4505-461b-98fe-0cf6cc5eaf44", newBlob->uuid());
  EXPECT_EQ("text/plain", newBlob->type());
  EXPECT_EQ(12u, newBlob->size());
}

TEST(V8ScriptValueSerializerTest, DecodeBlobIndexOutOfRange) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input =
      serializedValue({0xff, 0x09, 0x3f, 0x00, 0x69, 0x01});
  {
    V8ScriptValueDeserializer deserializer(scope.getScriptState(), input);
    ASSERT_TRUE(deserializer.deserialize()->IsNull());
  }
  {
    WebBlobInfoArray blobInfoArray;
    blobInfoArray.emplace_back("d875dfc2-4505-461b-98fe-0cf6cc5eaf44",
                               "text/plain", 12);
    V8ScriptValueDeserializer deserializer(scope.getScriptState(), input);
    deserializer.setBlobInfoArray(&blobInfoArray);
    ASSERT_TRUE(deserializer.deserialize()->IsNull());
  }
}

TEST(V8ScriptValueSerializerTest, RoundTripFileNative) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  File* file = File::create("/native/path");
  v8::Local<v8::Value> wrapper = toV8(file, scope.getScriptState());
  v8::Local<v8::Value> result = roundTrip(wrapper, scope);
  ASSERT_TRUE(V8File::hasInstance(result, scope.isolate()));
  File* newFile = V8File::toImpl(result.As<v8::Object>());
  EXPECT_TRUE(newFile->hasBackingFile());
  EXPECT_EQ("/native/path", newFile->path());
  EXPECT_TRUE(newFile->fileSystemURL().isEmpty());
}

TEST(V8ScriptValueSerializerTest, RoundTripFileBackedByBlob) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  const double modificationTime = 0.0;
  RefPtr<BlobDataHandle> blobDataHandle = BlobDataHandle::create();
  File* file = File::create("/native/path", modificationTime, blobDataHandle);
  v8::Local<v8::Value> wrapper = toV8(file, scope.getScriptState());
  v8::Local<v8::Value> result = roundTrip(wrapper, scope);
  ASSERT_TRUE(V8File::hasInstance(result, scope.isolate()));
  File* newFile = V8File::toImpl(result.As<v8::Object>());
  EXPECT_FALSE(newFile->hasBackingFile());
  EXPECT_TRUE(file->path().isEmpty());
  EXPECT_TRUE(newFile->fileSystemURL().isEmpty());
}

TEST(V8ScriptValueSerializerTest, RoundTripFileNativeSnapshot) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  FileMetadata metadata;
  metadata.platformPath = "/native/snapshot";
  File* file =
      File::createForFileSystemFile("name", metadata, File::IsUserVisible);
  v8::Local<v8::Value> wrapper = toV8(file, scope.getScriptState());
  v8::Local<v8::Value> result = roundTrip(wrapper, scope);
  ASSERT_TRUE(V8File::hasInstance(result, scope.isolate()));
  File* newFile = V8File::toImpl(result.As<v8::Object>());
  EXPECT_TRUE(newFile->hasBackingFile());
  EXPECT_EQ("/native/snapshot", newFile->path());
  EXPECT_TRUE(newFile->fileSystemURL().isEmpty());
}

TEST(V8ScriptValueSerializerTest, RoundTripFileNonNativeSnapshot) {
  // Preserving behavior, filesystem URL is not preserved across cloning.
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  KURL url(ParsedURLString,
           "filesystem:http://example.com/isolated/hash/non-native-file");
  File* file =
      File::createForFileSystemFile(url, FileMetadata(), File::IsUserVisible);
  v8::Local<v8::Value> wrapper = toV8(file, scope.getScriptState());
  v8::Local<v8::Value> result = roundTrip(wrapper, scope);
  ASSERT_TRUE(V8File::hasInstance(result, scope.isolate()));
  File* newFile = V8File::toImpl(result.As<v8::Object>());
  EXPECT_FALSE(newFile->hasBackingFile());
  EXPECT_TRUE(file->path().isEmpty());
  EXPECT_TRUE(newFile->fileSystemURL().isEmpty());
}

// Used for checking that times provided are between now and the current time
// when the checker was constructed, according to WTF::currentTime.
class TimeIntervalChecker {
 public:
  TimeIntervalChecker() : m_startTime(WTF::currentTime()) {}
  bool wasAliveAt(double timeInMilliseconds) {
    double time = timeInMilliseconds / msPerSecond;
    return m_startTime <= time && time <= WTF::currentTime();
  }

 private:
  const double m_startTime;
};

TEST(V8ScriptValueSerializerTest, DecodeFileV3) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  TimeIntervalChecker timeIntervalChecker;
  RefPtr<SerializedScriptValue> input = serializedValue(
      {0xff, 0x03, 0x3f, 0x00, 0x66, 0x04, 'p', 'a', 't', 'h', 0x24, 'f',
       '4',  'a',  '6',  'e',  'd',  'd',  '5', '-', '6', '5', 'a',  'd',
       '-',  '4',  'd',  'c',  '3',  '-',  'b', '6', '7', 'c', '-',  'a',
       '7',  '7',  '9',  'c',  '0',  '2',  'f', '0', 'f', 'a', '3',  0x0a,
       't',  'e',  'x',  't',  '/',  'p',  'l', 'a', 'i', 'n'});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.getScriptState(), input).deserialize();
  ASSERT_TRUE(V8File::hasInstance(result, scope.isolate()));
  File* newFile = V8File::toImpl(result.As<v8::Object>());
  EXPECT_EQ("path", newFile->path());
  EXPECT_EQ("f4a6edd5-65ad-4dc3-b67c-a779c02f0fa3", newFile->uuid());
  EXPECT_EQ("text/plain", newFile->type());
  EXPECT_FALSE(newFile->hasValidSnapshotMetadata());
  EXPECT_EQ(0u, newFile->size());
  EXPECT_TRUE(timeIntervalChecker.wasAliveAt(newFile->lastModifiedDate()));
  EXPECT_EQ(File::IsUserVisible, newFile->getUserVisibility());
}

TEST(V8ScriptValueSerializerTest, DecodeFileV4) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  TimeIntervalChecker timeIntervalChecker;
  RefPtr<SerializedScriptValue> input = serializedValue(
      {0xff, 0x04, 0x3f, 0x00, 0x66, 0x04, 'p', 'a',  't',  'h', 0x04, 'n',
       'a',  'm',  'e',  0x03, 'r',  'e',  'l', 0x24, 'f',  '4', 'a',  '6',
       'e',  'd',  'd',  '5',  '-',  '6',  '5', 'a',  'd',  '-', '4',  'd',
       'c',  '3',  '-',  'b',  '6',  '7',  'c', '-',  'a',  '7', '7',  '9',
       'c',  '0',  '2',  'f',  '0',  'f',  'a', '3',  0x0a, 't', 'e',  'x',
       't',  '/',  'p',  'l',  'a',  'i',  'n', 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.getScriptState(), input).deserialize();
  ASSERT_TRUE(V8File::hasInstance(result, scope.isolate()));
  File* newFile = V8File::toImpl(result.As<v8::Object>());
  EXPECT_EQ("path", newFile->path());
  EXPECT_EQ("name", newFile->name());
  EXPECT_EQ("rel", newFile->webkitRelativePath());
  EXPECT_EQ("f4a6edd5-65ad-4dc3-b67c-a779c02f0fa3", newFile->uuid());
  EXPECT_EQ("text/plain", newFile->type());
  EXPECT_FALSE(newFile->hasValidSnapshotMetadata());
  EXPECT_EQ(0u, newFile->size());
  EXPECT_TRUE(timeIntervalChecker.wasAliveAt(newFile->lastModifiedDate()));
  EXPECT_EQ(File::IsUserVisible, newFile->getUserVisibility());
}

TEST(V8ScriptValueSerializerTest, DecodeFileV4WithSnapshot) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input = serializedValue(
      {0xff, 0x04, 0x3f, 0x00, 0x66, 0x04, 'p', 'a',  't',  'h',  0x04, 'n',
       'a',  'm',  'e',  0x03, 'r',  'e',  'l', 0x24, 'f',  '4',  'a',  '6',
       'e',  'd',  'd',  '5',  '-',  '6',  '5', 'a',  'd',  '-',  '4',  'd',
       'c',  '3',  '-',  'b',  '6',  '7',  'c', '-',  'a',  '7',  '7',  '9',
       'c',  '0',  '2',  'f',  '0',  'f',  'a', '3',  0x0a, 't',  'e',  'x',
       't',  '/',  'p',  'l',  'a',  'i',  'n', 0x01, 0x80, 0x04, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0xd0, 0xbf});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.getScriptState(), input).deserialize();
  ASSERT_TRUE(V8File::hasInstance(result, scope.isolate()));
  File* newFile = V8File::toImpl(result.As<v8::Object>());
  EXPECT_EQ("path", newFile->path());
  EXPECT_EQ("name", newFile->name());
  EXPECT_EQ("rel", newFile->webkitRelativePath());
  EXPECT_EQ("f4a6edd5-65ad-4dc3-b67c-a779c02f0fa3", newFile->uuid());
  EXPECT_EQ("text/plain", newFile->type());
  EXPECT_TRUE(newFile->hasValidSnapshotMetadata());
  EXPECT_EQ(512u, newFile->size());
  // From v4 to v7, the last modified time is written in seconds.
  // So -0.25 represents 250 ms before the Unix epoch.
  EXPECT_EQ(-250.0, newFile->lastModifiedDate());
}

TEST(V8ScriptValueSerializerTest, DecodeFileV7) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  TimeIntervalChecker timeIntervalChecker;
  RefPtr<SerializedScriptValue> input = serializedValue(
      {0xff, 0x07, 0x3f, 0x00, 0x66, 0x04, 'p', 'a',  't',  'h', 0x04, 'n',
       'a',  'm',  'e',  0x03, 'r',  'e',  'l', 0x24, 'f',  '4', 'a',  '6',
       'e',  'd',  'd',  '5',  '-',  '6',  '5', 'a',  'd',  '-', '4',  'd',
       'c',  '3',  '-',  'b',  '6',  '7',  'c', '-',  'a',  '7', '7',  '9',
       'c',  '0',  '2',  'f',  '0',  'f',  'a', '3',  0x0a, 't', 'e',  'x',
       't',  '/',  'p',  'l',  'a',  'i',  'n', 0x00, 0x00, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.getScriptState(), input).deserialize();
  ASSERT_TRUE(V8File::hasInstance(result, scope.isolate()));
  File* newFile = V8File::toImpl(result.As<v8::Object>());
  EXPECT_EQ("path", newFile->path());
  EXPECT_EQ("name", newFile->name());
  EXPECT_EQ("rel", newFile->webkitRelativePath());
  EXPECT_EQ("f4a6edd5-65ad-4dc3-b67c-a779c02f0fa3", newFile->uuid());
  EXPECT_EQ("text/plain", newFile->type());
  EXPECT_FALSE(newFile->hasValidSnapshotMetadata());
  EXPECT_EQ(0u, newFile->size());
  EXPECT_TRUE(timeIntervalChecker.wasAliveAt(newFile->lastModifiedDate()));
  EXPECT_EQ(File::IsNotUserVisible, newFile->getUserVisibility());
}

TEST(V8ScriptValueSerializerTest, DecodeFileV8WithSnapshot) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input = serializedValue(
      {0xff, 0x08, 0x3f, 0x00, 0x66, 0x04, 'p',  'a',  't',  'h',  0x04, 'n',
       'a',  'm',  'e',  0x03, 'r',  'e',  'l',  0x24, 'f',  '4',  'a',  '6',
       'e',  'd',  'd',  '5',  '-',  '6',  '5',  'a',  'd',  '-',  '4',  'd',
       'c',  '3',  '-',  'b',  '6',  '7',  'c',  '-',  'a',  '7',  '7',  '9',
       'c',  '0',  '2',  'f',  '0',  'f',  'a',  '3',  0x0a, 't',  'e',  'x',
       't',  '/',  'p',  'l',  'a',  'i',  'n',  0x01, 0x80, 0x04, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0xd0, 0xbf, 0x01, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.getScriptState(), input).deserialize();
  ASSERT_TRUE(V8File::hasInstance(result, scope.isolate()));
  File* newFile = V8File::toImpl(result.As<v8::Object>());
  EXPECT_EQ("path", newFile->path());
  EXPECT_EQ("name", newFile->name());
  EXPECT_EQ("rel", newFile->webkitRelativePath());
  EXPECT_EQ("f4a6edd5-65ad-4dc3-b67c-a779c02f0fa3", newFile->uuid());
  EXPECT_EQ("text/plain", newFile->type());
  EXPECT_TRUE(newFile->hasValidSnapshotMetadata());
  EXPECT_EQ(512u, newFile->size());
  // From v8, the last modified time is written in milliseconds.
  // So -0.25 represents 0.25 ms before the Unix epoch.
  EXPECT_EQ(-0.25, newFile->lastModifiedDate());
  EXPECT_EQ(File::IsUserVisible, newFile->getUserVisibility());
}

TEST(V8ScriptValueSerializerTest, RoundTripFileIndex) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  File* file = File::create("/native/path");
  v8::Local<v8::Value> wrapper = toV8(file, scope.getScriptState());
  WebBlobInfoArray blobInfoArray;
  v8::Local<v8::Value> result =
      roundTrip(wrapper, scope, nullptr, nullptr, &blobInfoArray);

  // As above, the resulting blob should be correct.
  ASSERT_TRUE(V8File::hasInstance(result, scope.isolate()));
  File* newFile = V8File::toImpl(result.As<v8::Object>());
  EXPECT_TRUE(newFile->hasBackingFile());
  EXPECT_EQ("/native/path", newFile->path());
  EXPECT_TRUE(newFile->fileSystemURL().isEmpty());

  // The blob info array should also contain the details since it was serialized
  // by index into this array.
  ASSERT_EQ(1u, blobInfoArray.size());
  const WebBlobInfo& info = blobInfoArray[0];
  EXPECT_TRUE(info.isFile());
  EXPECT_EQ("/native/path", info.filePath());
  EXPECT_EQ(file->uuid(), String(info.uuid()));
}

TEST(V8ScriptValueSerializerTest, DecodeFileIndex) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input =
      serializedValue({0xff, 0x09, 0x3f, 0x00, 0x65, 0x00});
  WebBlobInfoArray blobInfoArray;
  blobInfoArray.emplace_back("d875dfc2-4505-461b-98fe-0cf6cc5eaf44",
                             "/native/path", "path", "text/plain");
  V8ScriptValueDeserializer deserializer(scope.getScriptState(), input);
  deserializer.setBlobInfoArray(&blobInfoArray);
  v8::Local<v8::Value> result = deserializer.deserialize();
  ASSERT_TRUE(V8File::hasInstance(result, scope.isolate()));
  File* newFile = V8File::toImpl(result.As<v8::Object>());
  EXPECT_EQ("d875dfc2-4505-461b-98fe-0cf6cc5eaf44", newFile->uuid());
  EXPECT_EQ("text/plain", newFile->type());
  EXPECT_EQ("/native/path", newFile->path());
  EXPECT_EQ("path", newFile->name());
}

TEST(V8ScriptValueSerializerTest, DecodeFileIndexOutOfRange) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input =
      serializedValue({0xff, 0x09, 0x3f, 0x00, 0x65, 0x01});
  {
    V8ScriptValueDeserializer deserializer(scope.getScriptState(), input);
    ASSERT_TRUE(deserializer.deserialize()->IsNull());
  }
  {
    WebBlobInfoArray blobInfoArray;
    blobInfoArray.emplace_back("d875dfc2-4505-461b-98fe-0cf6cc5eaf44",
                               "/native/path", "path", "text/plain");
    V8ScriptValueDeserializer deserializer(scope.getScriptState(), input);
    deserializer.setBlobInfoArray(&blobInfoArray);
    ASSERT_TRUE(deserializer.deserialize()->IsNull());
  }
}

// Most of the logic for FileList is shared with File, so the tests here are
// fairly basic.

TEST(V8ScriptValueSerializerTest, RoundTripFileList) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  FileList* fileList = FileList::create();
  fileList->append(File::create("/native/path"));
  fileList->append(File::create("/native/path2"));
  v8::Local<v8::Value> wrapper = toV8(fileList, scope.getScriptState());
  v8::Local<v8::Value> result = roundTrip(wrapper, scope);
  ASSERT_TRUE(V8FileList::hasInstance(result, scope.isolate()));
  FileList* newFileList = V8FileList::toImpl(result.As<v8::Object>());
  ASSERT_EQ(2u, newFileList->length());
  EXPECT_EQ("/native/path", newFileList->item(0)->path());
  EXPECT_EQ("/native/path2", newFileList->item(1)->path());
}

TEST(V8ScriptValueSerializerTest, DecodeEmptyFileList) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input =
      serializedValue({0xff, 0x09, 0x3f, 0x00, 0x6c, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.getScriptState(), input).deserialize();
  ASSERT_TRUE(V8FileList::hasInstance(result, scope.isolate()));
  FileList* newFileList = V8FileList::toImpl(result.As<v8::Object>());
  EXPECT_EQ(0u, newFileList->length());
}

TEST(V8ScriptValueSerializerTest, DecodeFileListWithInvalidLength) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input =
      serializedValue({0xff, 0x09, 0x3f, 0x00, 0x6c, 0x01});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.getScriptState(), input).deserialize();
  EXPECT_TRUE(result->IsNull());
}

TEST(V8ScriptValueSerializerTest, DecodeFileListV8WithoutSnapshot) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  TimeIntervalChecker timeIntervalChecker;
  RefPtr<SerializedScriptValue> input = serializedValue(
      {0xff, 0x08, 0x3f, 0x00, 0x6c, 0x01, 0x04, 'p', 'a',  't',  'h', 0x04,
       'n',  'a',  'm',  'e',  0x03, 'r',  'e',  'l', 0x24, 'f',  '4', 'a',
       '6',  'e',  'd',  'd',  '5',  '-',  '6',  '5', 'a',  'd',  '-', '4',
       'd',  'c',  '3',  '-',  'b',  '6',  '7',  'c', '-',  'a',  '7', '7',
       '9',  'c',  '0',  '2',  'f',  '0',  'f',  'a', '3',  0x0a, 't', 'e',
       'x',  't',  '/',  'p',  'l',  'a',  'i',  'n', 0x00, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.getScriptState(), input).deserialize();
  ASSERT_TRUE(V8FileList::hasInstance(result, scope.isolate()));
  FileList* newFileList = V8FileList::toImpl(result.As<v8::Object>());
  EXPECT_EQ(1u, newFileList->length());
  File* newFile = newFileList->item(0);
  EXPECT_EQ("path", newFile->path());
  EXPECT_EQ("name", newFile->name());
  EXPECT_EQ("rel", newFile->webkitRelativePath());
  EXPECT_EQ("f4a6edd5-65ad-4dc3-b67c-a779c02f0fa3", newFile->uuid());
  EXPECT_EQ("text/plain", newFile->type());
  EXPECT_FALSE(newFile->hasValidSnapshotMetadata());
  EXPECT_EQ(0u, newFile->size());
  EXPECT_TRUE(timeIntervalChecker.wasAliveAt(newFile->lastModifiedDate()));
  EXPECT_EQ(File::IsNotUserVisible, newFile->getUserVisibility());
}

TEST(V8ScriptValueSerializerTest, RoundTripFileListIndex) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  FileList* fileList = FileList::create();
  fileList->append(File::create("/native/path"));
  fileList->append(File::create("/native/path2"));
  v8::Local<v8::Value> wrapper = toV8(fileList, scope.getScriptState());
  WebBlobInfoArray blobInfoArray;
  v8::Local<v8::Value> result =
      roundTrip(wrapper, scope, nullptr, nullptr, &blobInfoArray);

  // FileList should be produced correctly.
  ASSERT_TRUE(V8FileList::hasInstance(result, scope.isolate()));
  FileList* newFileList = V8FileList::toImpl(result.As<v8::Object>());
  ASSERT_EQ(2u, newFileList->length());
  EXPECT_EQ("/native/path", newFileList->item(0)->path());
  EXPECT_EQ("/native/path2", newFileList->item(1)->path());

  // And the blob info array should be populated.
  ASSERT_EQ(2u, blobInfoArray.size());
  EXPECT_TRUE(blobInfoArray[0].isFile());
  EXPECT_EQ("/native/path", blobInfoArray[0].filePath());
  EXPECT_TRUE(blobInfoArray[1].isFile());
  EXPECT_EQ("/native/path2", blobInfoArray[1].filePath());
}

TEST(V8ScriptValueSerializerTest, DecodeEmptyFileListIndex) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input =
      serializedValue({0xff, 0x09, 0x3f, 0x00, 0x4c, 0x00});
  WebBlobInfoArray blobInfoArray;
  V8ScriptValueDeserializer deserializer(scope.getScriptState(), input);
  deserializer.setBlobInfoArray(&blobInfoArray);
  v8::Local<v8::Value> result = deserializer.deserialize();
  ASSERT_TRUE(V8FileList::hasInstance(result, scope.isolate()));
  FileList* newFileList = V8FileList::toImpl(result.As<v8::Object>());
  EXPECT_EQ(0u, newFileList->length());
}

TEST(V8ScriptValueSerializerTest, DecodeFileListIndexWithInvalidLength) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input =
      serializedValue({0xff, 0x09, 0x3f, 0x00, 0x4c, 0x02});
  WebBlobInfoArray blobInfoArray;
  V8ScriptValueDeserializer deserializer(scope.getScriptState(), input);
  deserializer.setBlobInfoArray(&blobInfoArray);
  v8::Local<v8::Value> result = deserializer.deserialize();
  EXPECT_TRUE(result->IsNull());
}

TEST(V8ScriptValueSerializerTest, DecodeFileListIndex) {
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input =
      serializedValue({0xff, 0x09, 0x3f, 0x00, 0x4c, 0x01, 0x00, 0x00});
  WebBlobInfoArray blobInfoArray;
  blobInfoArray.emplace_back("d875dfc2-4505-461b-98fe-0cf6cc5eaf44",
                             "/native/path", "name", "text/plain");
  V8ScriptValueDeserializer deserializer(scope.getScriptState(), input);
  deserializer.setBlobInfoArray(&blobInfoArray);
  v8::Local<v8::Value> result = deserializer.deserialize();
  FileList* newFileList = V8FileList::toImpl(result.As<v8::Object>());
  EXPECT_EQ(1u, newFileList->length());
  File* newFile = newFileList->item(0);
  EXPECT_EQ("/native/path", newFile->path());
  EXPECT_EQ("name", newFile->name());
  EXPECT_EQ("d875dfc2-4505-461b-98fe-0cf6cc5eaf44", newFile->uuid());
  EXPECT_EQ("text/plain", newFile->type());
}

class ScopedEnableCompositorWorker {
 public:
  ScopedEnableCompositorWorker()
      : m_wasEnabled(RuntimeEnabledFeatures::compositorWorkerEnabled()) {
    RuntimeEnabledFeatures::setCompositorWorkerEnabled(true);
  }
  ~ScopedEnableCompositorWorker() {
    RuntimeEnabledFeatures::setCompositorWorkerEnabled(m_wasEnabled);
  }

 private:
  bool m_wasEnabled;
};

TEST(V8ScriptValueSerializerTest, RoundTripCompositorProxy) {
  ScopedEnableCompositorWorker enableCompositorWorker;
  ScopedEnableV8BasedStructuredClone enable;
  V8TestingScope scope;
  HTMLElement* element = scope.document().body();
  Vector<String> properties{"transform"};
  CompositorProxy* proxy = CompositorProxy::create(
      scope.getExecutionContext(), element, properties, ASSERT_NO_EXCEPTION);
  uint64_t elementId = proxy->elementId();

  v8::Local<v8::Value> wrapper = toV8(proxy, scope.getScriptState());
  v8::Local<v8::Value> result = roundTrip(wrapper, scope);
  ASSERT_TRUE(V8CompositorProxy::hasInstance(result, scope.isolate()));
  CompositorProxy* newProxy =
      V8CompositorProxy::toImpl(result.As<v8::Object>());
  EXPECT_EQ(elementId, newProxy->elementId());
  EXPECT_EQ(CompositorMutableProperty::kTransform,
            newProxy->compositorMutableProperties());
}

// Decode tests aren't included here because they're slightly non-trivial (an
// element with the right ID must actually exist) and this feature is both
// unshipped and likely to not use this mechanism when it does.
// TODO(jbroman): Update this if that turns out not to be the case.

}  // namespace
}  // namespace blink
