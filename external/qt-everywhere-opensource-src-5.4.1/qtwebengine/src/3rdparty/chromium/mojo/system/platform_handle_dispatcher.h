// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_PLATFORM_HANDLE_DISPATCHER_H_
#define MOJO_SYSTEM_PLATFORM_HANDLE_DISPATCHER_H_

#include "base/macros.h"
#include "mojo/embedder/scoped_platform_handle.h"
#include "mojo/system/simple_dispatcher.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

// A dispatcher that simply wraps/transports a |PlatformHandle| (only for use by
// the embedder).
class MOJO_SYSTEM_IMPL_EXPORT PlatformHandleDispatcher
    : public SimpleDispatcher {
 public:
  explicit PlatformHandleDispatcher(
      embedder::ScopedPlatformHandle platform_handle);

  embedder::ScopedPlatformHandle PassPlatformHandle();

  // |Dispatcher| public methods:
  virtual Type GetType() const OVERRIDE;

  // The "opposite" of |SerializeAndClose()|. (Typically this is called by
  // |Dispatcher::Deserialize()|.)
  static scoped_refptr<PlatformHandleDispatcher> Deserialize(
      Channel* channel,
      const void* source,
      size_t size,
      embedder::PlatformHandleVector* platform_handles);

 private:
  virtual ~PlatformHandleDispatcher();

  // |Dispatcher| protected methods:
  virtual void CloseImplNoLock() OVERRIDE;
  virtual scoped_refptr<Dispatcher>
      CreateEquivalentDispatcherAndCloseImplNoLock() OVERRIDE;
  virtual void StartSerializeImplNoLock(Channel* channel,
                                        size_t* max_size,
                                        size_t* max_platform_handles) OVERRIDE;
  virtual bool EndSerializeAndCloseImplNoLock(
      Channel* channel,
      void* destination,
      size_t* actual_size,
      embedder::PlatformHandleVector* platform_handles) OVERRIDE;

  // |SimpleDispatcher| method:
  virtual HandleSignalsState GetHandleSignalsStateNoLock() const OVERRIDE;

  embedder::ScopedPlatformHandle platform_handle_;

  DISALLOW_COPY_AND_ASSIGN(PlatformHandleDispatcher);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_PLATFORM_HANDLE_DISPATCHER_H_
