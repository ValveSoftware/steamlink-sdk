// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BytesConsumer_h
#define BytesConsumer_h

#include "modules/ModulesExport.h"
#include "platform/blob/BlobData.h"
#include "platform/heap/Handle.h"
#include "platform/network/EncodedFormData.h"
#include "wtf/Compiler.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;

// BytesConsumer represents the "consumer" side of a data pipe. A user
// can read data from it.
//
// A BytesConsumer is bound to the thread on which it is created.
// BytesConsumer has four states: waiting, readable, closed and errored. Once
// the state becomes closed or errored, it will never change. |readable| means
// that the BytesConsumer is ready to read non-empty bytes synchronously.
class MODULES_EXPORT BytesConsumer
    : public GarbageCollectedFinalized<BytesConsumer> {
 public:
  enum class Result {
    Ok,
    ShouldWait,
    Done,
    Error,
  };
  // Readable and Waiting are indistinguishable from BytesConsumer users.
  enum class PublicState {
    ReadableOrWaiting,
    Closed,
    Errored,
  };
  enum class BlobSizePolicy {
    // The returned blob must have a valid size (i.e. != kuint64max).
    DisallowBlobWithInvalidSize,
    // The returned blob can have an invalid size.
    AllowBlobWithInvalidSize
  };
  class MODULES_EXPORT Error {
   public:
    Error() {}
    explicit Error(const String& message) : m_message(message) {}
    const String& message() const { return m_message; }
    bool operator==(const Error& e) const { return e.m_message == m_message; }

   private:
    String m_message;
  };
  // Client gets notification from the associated ByteConsumer.
  class MODULES_EXPORT Client : public GarbageCollectedMixin {
   public:
    virtual ~Client() {}

    // This function is called when the state changes (e.g., readable =>
    // errored). This function can be called more than needed, i.e., it can
    // be called even when the state is not actually changed, but it is
    // guaranteed that this function cannot be called after the state
    // becomes closed or errored.
    //
    // This function is not called when the state change is trigerred by
    // public methods called by a user. For example, when a user reads
    // data by |read| and the state changes from waiting to readable, this
    // function will not be called.
    virtual void onStateChange() = 0;
  };

  virtual ~BytesConsumer() {}

  // Begins a two-phase read. On success, the function stores a buffer
  // that contains the read data of length |*available| into |*buffer|.
  // Returns Ok when readable.
  // Returns ShouldWait when it's waiting.
  // Returns Done when it's closed.
  // Returns Error when errored.
  // When not readable, the caller don't have to (and must not) call
  // endRead, because the read session implicitly ends in that case.
  //
  // |*buffer| will become invalid when this object becomes unreachable,
  // even if endRead is not called.
  //
  // |*buffer| will be set to null and |*available| will be set to 0 if not
  // readable.
  virtual Result beginRead(const char** buffer,
                           size_t* available) WARN_UNUSED_RESULT = 0;

  // Ends a two-phase read.
  // This function can modify this BytesConsumer's state.
  // Returns Ok when the consumer stays readable or waiting.
  // Returns Done when it's closed.
  // Returns Error when it's errored.
  virtual Result endRead(size_t readSize) WARN_UNUSED_RESULT = 0;

  // Drains the data as a BlobDataHandle.
  // When this function returns a non-null value, the returned blob handle
  // contains bytes that would be read through beginRead and
  // endRead functions without calling this function. In such a case, this
  // object becomes closed.
  // When this function returns null value, this function does nothing.
  // When |policy| is DisallowBlobWithInvalidSize, this function doesn't
  // return a non-null blob handle with unspecified size.
  // The type of the returned blob handle may not be meaningful.
  virtual PassRefPtr<BlobDataHandle> drainAsBlobDataHandle(
      BlobSizePolicy = BlobSizePolicy::DisallowBlobWithInvalidSize) {
    return nullptr;
  }

  // Drains the data as an EncodedFormData.
  // When this function returns a non-null value, the returned form data
  // contains bytes that would be read through beginRead and
  // endRead functions without calling this function. In such a case, this
  // object becomes closed.
  // When this function returns null value, this function does nothing.
  // This function returns a non-null form data when the handle is made
  // from an EncodedFormData-convertible value.
  virtual PassRefPtr<EncodedFormData> drainAsFormData() { return nullptr; }

  // Sets a client. This can be called only when no client is set. When
  // this object is already closed or errored, this function does nothing.
  virtual void setClient(Client*) = 0;
  // Clears the set client.
  // A client will be implicitly cleared when this object gets closed or
  // errored (after the state change itself is notified).
  virtual void clearClient() = 0;

  // Cancels this ByteConsumer. This function does nothing when |this| is
  // already closed or errored. Otherwise, this object becomes closed.
  // This function cannot be called in a two-phase read.
  virtual void cancel() = 0;

  // Returns the current state.
  virtual PublicState getPublicState() const = 0;

  // Returns the associated error of this object. This function can be called
  // only when errored.
  virtual Error getError() const = 0;

  // Each implementation should return a string that represents the
  // implementation for debug purpose.
  virtual String debugName() const = 0;

  // Creates two BytesConsumer both of which represent the data sequence that
  // would be read from |src| and store them to |*dest1| and |*dest2|.
  // |src| must not have a client when called.
  static void tee(ExecutionContext*,
                  BytesConsumer* src,
                  BytesConsumer** dest1,
                  BytesConsumer** dest2);

  // Returns a BytesConsumer whose state is Closed.
  static BytesConsumer* createClosed();

  // Returns a BytesConsumer whose state is Errored.
  static BytesConsumer* createErrored(const Error&);

  DEFINE_INLINE_VIRTUAL_TRACE() {}

 protected:
  // This InternalState directly corresponds to the states in the class
  // comments. This enum is defined here for subclasses.
  enum class InternalState {
    Readable,
    Waiting,
    Closed,
    Errored,
  };

  static PublicState getPublicStateFromInternalState(InternalState state) {
    switch (state) {
      case InternalState::Readable:
      case InternalState::Waiting:
        return PublicState::ReadableOrWaiting;
      case InternalState::Closed:
        return PublicState::Closed;
      case InternalState::Errored:
        return PublicState::Errored;
    }
    NOTREACHED();
    return PublicState::ReadableOrWaiting;
  }
};

}  // namespace blink

#endif  // BytesConsumer_h
