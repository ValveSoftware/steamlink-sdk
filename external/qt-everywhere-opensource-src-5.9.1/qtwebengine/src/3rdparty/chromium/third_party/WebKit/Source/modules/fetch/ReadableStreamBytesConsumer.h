// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReadableStreamBytesConsumer_h
#define ReadableStreamBytesConsumer_h

#include "bindings/core/v8/ScriptValue.h"
#include "core/dom/DOMTypedArray.h"
#include "modules/ModulesExport.h"
#include "modules/fetch/BytesConsumer.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

class ScriptState;

// This class is a BytesConsumer pulling bytes from ReadableStream
// implemented with V8 Extras.
// The stream will be immediately locked by the consumer and will never be
// released.
//
// The ReadableStreamReader handle held in a ReadableStreamDataConsumerHandle
// is weak. A user must guarantee that the ReadableStreamReader object is kept
// alive appropriately.
class MODULES_EXPORT ReadableStreamBytesConsumer final : public BytesConsumer {
  WTF_MAKE_NONCOPYABLE(ReadableStreamBytesConsumer);
  USING_PRE_FINALIZER(ReadableStreamBytesConsumer, dispose);

 public:
  ReadableStreamBytesConsumer(ScriptState*, ScriptValue streamReader);
  ~ReadableStreamBytesConsumer() override;

  Result beginRead(const char** buffer, size_t* available) override;
  Result endRead(size_t readSize) override;
  void setClient(BytesConsumer::Client*) override;
  void clearClient() override;

  void cancel() override;
  PublicState getPublicState() const override;
  Error getError() const override;
  String debugName() const override { return "ReadableStreamBytesConsumer"; }

  DECLARE_TRACE();

 private:
  class OnFulfilled;
  class OnRejected;

  void dispose();
  void onRead(DOMUint8Array*);
  void onReadDone();
  void onRejected();
  void notify();

  // |m_reader| is a weak persistent. It should be kept alive by someone
  // outside of ReadableStreamBytesConsumer.
  // Holding a ScopedPersistent here is safe in terms of cross-world wrapper
  // leakage because we read only Uint8Array chunks from the reader.
  ScopedPersistent<v8::Value> m_reader;
  RefPtr<ScriptState> m_scriptState;
  Member<BytesConsumer::Client> m_client;
  Member<DOMUint8Array> m_pendingBuffer;
  size_t m_pendingOffset = 0;
  PublicState m_state = PublicState::ReadableOrWaiting;
  bool m_isReading = false;
};

}  // namespace blink

#endif  // ReadableStreamBytesConsumer_h
