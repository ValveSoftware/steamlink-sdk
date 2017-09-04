// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_DISPATCHER_H_
#define MOJO_EDK_SYSTEM_DISPATCHER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <ostream>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "mojo/edk/embedder/platform_handle.h"
#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/system/handle_signals_state.h"
#include "mojo/edk/system/ports/name.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/edk/system/watcher.h"
#include "mojo/public/c/system/buffer.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/c/system/types.h"

namespace mojo {
namespace edk {

class Awakable;
class Dispatcher;
class MessageForTransit;

using DispatcherVector = std::vector<scoped_refptr<Dispatcher>>;

// A |Dispatcher| implements Mojo EDK calls that are associated with a
// particular MojoHandle, with the exception of MojoWait and MojoWaitMany (
// which are implemented directly in Core.).
class MOJO_SYSTEM_IMPL_EXPORT Dispatcher
    : public base::RefCountedThreadSafe<Dispatcher> {
 public:
  struct DispatcherInTransit {
    DispatcherInTransit();
    DispatcherInTransit(const DispatcherInTransit& other);
    ~DispatcherInTransit();

    scoped_refptr<Dispatcher> dispatcher;
    MojoHandle local_handle;
  };

  enum class Type {
    UNKNOWN = 0,
    MESSAGE_PIPE,
    DATA_PIPE_PRODUCER,
    DATA_PIPE_CONSUMER,
    SHARED_BUFFER,
    WAIT_SET,

    // "Private" types (not exposed via the public interface):
    PLATFORM_HANDLE = -1,
  };

  // All Dispatchers must minimally implement these methods.

  virtual Type GetType() const = 0;
  virtual MojoResult Close() = 0;

  ///////////// Watch API ////////////////////

  virtual MojoResult Watch(MojoHandleSignals signals,
                           const Watcher::WatchCallback& callback,
                           uintptr_t context);

  virtual MojoResult CancelWatch(uintptr_t context);

  ///////////// Message pipe API /////////////

  virtual MojoResult WriteMessage(std::unique_ptr<MessageForTransit> message,
                                  MojoWriteMessageFlags flags);

  virtual MojoResult ReadMessage(std::unique_ptr<MessageForTransit>* message,
                                 uint32_t* num_bytes,
                                 MojoHandle* handles,
                                 uint32_t* num_handles,
                                 MojoReadMessageFlags flags,
                                 bool read_any_size);

  ///////////// Shared buffer API /////////////

  // |options| may be null. |new_dispatcher| must not be null, but
  // |*new_dispatcher| should be null (and will contain the dispatcher for the
  // new handle on success).
  virtual MojoResult DuplicateBufferHandle(
      const MojoDuplicateBufferHandleOptions* options,
      scoped_refptr<Dispatcher>* new_dispatcher);

  virtual MojoResult MapBuffer(
      uint64_t offset,
      uint64_t num_bytes,
      MojoMapBufferFlags flags,
      std::unique_ptr<PlatformSharedBufferMapping>* mapping);

  ///////////// Data pipe consumer API /////////////

  virtual MojoResult ReadData(void* elements,
                              uint32_t* num_bytes,
                              MojoReadDataFlags flags);

  virtual MojoResult BeginReadData(const void** buffer,
                                   uint32_t* buffer_num_bytes,
                                   MojoReadDataFlags flags);

  virtual MojoResult EndReadData(uint32_t num_bytes_read);

  ///////////// Data pipe producer API /////////////

  virtual MojoResult WriteData(const void* elements,
                               uint32_t* num_bytes,
                               MojoWriteDataFlags flags);

  virtual MojoResult BeginWriteData(void** buffer,
                                    uint32_t* buffer_num_bytes,
                                    MojoWriteDataFlags flags);

  virtual MojoResult EndWriteData(uint32_t num_bytes_written);

  ///////////// Wait set API /////////////

  // Adds a dispatcher to wait on. When the dispatcher satisfies |signals|, it
  // will be returned in the next call to |GetReadyDispatchers()|. If
  // |dispatcher| has been added, it must be removed before adding again,
  // otherwise |MOJO_RESULT_ALREADY_EXISTS| will be returned.
  virtual MojoResult AddWaitingDispatcher(
      const scoped_refptr<Dispatcher>& dispatcher,
      MojoHandleSignals signals,
      uintptr_t context);

  // Removes a dispatcher to wait on. If |dispatcher| has not been added,
  // |MOJO_RESULT_NOT_FOUND| will be returned.
  virtual MojoResult RemoveWaitingDispatcher(
      const scoped_refptr<Dispatcher>& dispatcher);

  // Returns a set of ready dispatchers. |*count| is the maximum number of
  // dispatchers to return, and will contain the number of dispatchers returned
  // in |dispatchers| on completion.
  virtual MojoResult GetReadyDispatchers(uint32_t* count,
                                         DispatcherVector* dispatchers,
                                         MojoResult* results,
                                         uintptr_t* contexts);

  ///////////// General-purpose API for all handle types /////////

  // Gets the current handle signals state. (The default implementation simply
  // returns a default-constructed |HandleSignalsState|, i.e., no signals
  // satisfied or satisfiable.) Note: The state is subject to change from other
  // threads.
  virtual HandleSignalsState GetHandleSignalsState() const;

  // Adds an awakable to this dispatcher, which will be woken up when this
  // object changes state to satisfy |signals| with context |context|. It will
  // also be woken up when it becomes impossible for the object to ever satisfy
  // |signals| with a suitable error status.
  //
  // If |signals_state| is non-null, on *failure* |*signals_state| will be set
  // to the current handle signals state (on success, it is left untouched).
  //
  // Returns:
  //  - |MOJO_RESULT_OK| if the awakable was added;
  //  - |MOJO_RESULT_ALREADY_EXISTS| if |signals| is already satisfied;
  //  - |MOJO_RESULT_INVALID_ARGUMENT| if the dispatcher has been closed; and
  //  - |MOJO_RESULT_FAILED_PRECONDITION| if it is not (or no longer) possible
  //    that |signals| will ever be satisfied.
  virtual MojoResult AddAwakable(Awakable* awakable,
                                 MojoHandleSignals signals,
                                 uintptr_t context,
                                 HandleSignalsState* signals_state);

  // Removes an awakable from this dispatcher. (It is valid to call this
  // multiple times for the same |awakable| on the same object, so long as
  // |AddAwakable()| was called at most once.) If |signals_state| is non-null,
  // |*signals_state| will be set to the current handle signals state.
  virtual void RemoveAwakable(Awakable* awakable,
                              HandleSignalsState* signals_state);

  // Informs the caller of the total serialized size (in bytes) and the total
  // number of platform handles and ports needed to transfer this dispatcher
  // across a message pipe.
  //
  // Must eventually be followed by a call to EndSerializeAndClose(). Note that
  // StartSerialize() and EndSerialize() are always called in sequence, and
  // only between calls to BeginTransit() and either (but not both)
  // CompleteTransitAndClose() or CancelTransit().
  //
  // For this reason it is IMPERATIVE that the implementation ensure a
  // consistent serializable state between BeginTransit() and
  // CompleteTransitAndClose()/CancelTransit().
  virtual void StartSerialize(uint32_t* num_bytes,
                              uint32_t* num_ports,
                              uint32_t* num_platform_handles);

  // Serializes this dispatcher into |destination|, |ports|, and |handles|.
  // Returns true iff successful, false otherwise. In either case the dispatcher
  // will close.
  //
  // NOTE: Transit MAY still fail after this call returns. Implementations
  // should not assume PlatformHandle ownership has transferred until
  // CompleteTransitAndClose() is called. In other words, if CancelTransit() is
  // called, the implementation should retain its PlatformHandles in working
  // condition.
  virtual bool EndSerialize(void* destination,
                            ports::PortName* ports,
                            PlatformHandle* handles);

  // Does whatever is necessary to begin transit of the dispatcher.  This
  // should return |true| if transit is OK, or false if the underlying resource
  // is deemed busy by the implementation.
  virtual bool BeginTransit();

  // Does whatever is necessary to complete transit of the dispatcher, including
  // closure. This is only called upon successfully transmitting an outgoing
  // message containing this serialized dispatcher.
  virtual void CompleteTransitAndClose();

  // Does whatever is necessary to cancel transit of the dispatcher. The
  // dispatcher should remain in a working state and resume normal operation.
  virtual void CancelTransit();

  // Deserializes a specific dispatcher type from an incoming message.
  static scoped_refptr<Dispatcher> Deserialize(
      Type type,
      const void* bytes,
      size_t num_bytes,
      const ports::PortName* ports,
      size_t num_ports,
      PlatformHandle* platform_handles,
      size_t num_platform_handles);

 protected:
  friend class base::RefCountedThreadSafe<Dispatcher>;

  Dispatcher();
  virtual ~Dispatcher();

  DISALLOW_COPY_AND_ASSIGN(Dispatcher);
};

// So logging macros and |DCHECK_EQ()|, etc. work.
MOJO_SYSTEM_IMPL_EXPORT inline std::ostream& operator<<(std::ostream& out,
                                                        Dispatcher::Type type) {
  return out << static_cast<int>(type);
}

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_DISPATCHER_H_
