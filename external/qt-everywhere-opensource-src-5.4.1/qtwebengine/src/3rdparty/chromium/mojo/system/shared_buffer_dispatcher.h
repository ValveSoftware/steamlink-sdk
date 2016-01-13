// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_SHARED_BUFFER_DISPATCHER_H_
#define MOJO_SYSTEM_SHARED_BUFFER_DISPATCHER_H_

#include "base/macros.h"
#include "mojo/system/raw_shared_buffer.h"
#include "mojo/system/simple_dispatcher.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

// TODO(vtl): We derive from SimpleDispatcher, even though we don't currently
// have anything that's waitable. I want to add a "transferrable" wait flag.
class MOJO_SYSTEM_IMPL_EXPORT SharedBufferDispatcher : public SimpleDispatcher {
 public:
  // The default options to use for |MojoCreateSharedBuffer()|. (Real uses
  // should obtain this via |ValidateCreateOptions()| with a null |in_options|;
  // this is exposed directly for testing convenience.)
  static const MojoCreateSharedBufferOptions kDefaultCreateOptions;

  // Validates and/or sets default options for |MojoCreateSharedBufferOptions|.
  // If non-null, |in_options| must point to a struct of at least
  // |in_options->struct_size| bytes. |out_options| must point to a (current)
  // |MojoCreateSharedBufferOptions| and will be entirely overwritten on success
  // (it may be partly overwritten on failure).
  static MojoResult ValidateCreateOptions(
      const MojoCreateSharedBufferOptions* in_options,
      MojoCreateSharedBufferOptions* out_options);

  // Static factory method: |validated_options| must be validated (obviously).
  // On failure, |*result| will be left as-is.
  static MojoResult Create(
      const MojoCreateSharedBufferOptions& validated_options,
      uint64_t num_bytes,
      scoped_refptr<SharedBufferDispatcher>* result);

  // |Dispatcher| public methods:
  virtual Type GetType() const OVERRIDE;

  // The "opposite" of |SerializeAndClose()|. (Typically this is called by
  // |Dispatcher::Deserialize()|.)
  static scoped_refptr<SharedBufferDispatcher> Deserialize(
      Channel* channel,
      const void* source,
      size_t size,
      embedder::PlatformHandleVector* platform_handles);

 private:
  explicit SharedBufferDispatcher(
      scoped_refptr<RawSharedBuffer> shared_buffer_);
  virtual ~SharedBufferDispatcher();

  // Validates and/or sets default options for
  // |MojoDuplicateBufferHandleOptions|. If non-null, |in_options| must point to
  // a struct of at least |in_options->struct_size| bytes. |out_options| must
  // point to a (current) |MojoDuplicateBufferHandleOptions| and will be
  // entirely overwritten on success (it may be partly overwritten on failure).
  static MojoResult ValidateDuplicateOptions(
      const MojoDuplicateBufferHandleOptions* in_options,
      MojoDuplicateBufferHandleOptions* out_options);

  // |Dispatcher| protected methods:
  virtual void CloseImplNoLock() OVERRIDE;
  virtual scoped_refptr<Dispatcher>
      CreateEquivalentDispatcherAndCloseImplNoLock() OVERRIDE;
  virtual MojoResult DuplicateBufferHandleImplNoLock(
      const MojoDuplicateBufferHandleOptions* options,
      scoped_refptr<Dispatcher>* new_dispatcher) OVERRIDE;
  virtual MojoResult MapBufferImplNoLock(
      uint64_t offset,
      uint64_t num_bytes,
      MojoMapBufferFlags flags,
      scoped_ptr<RawSharedBufferMapping>* mapping) OVERRIDE;
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

  scoped_refptr<RawSharedBuffer> shared_buffer_;

  DISALLOW_COPY_AND_ASSIGN(SharedBufferDispatcher);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_SHARED_BUFFER_DISPATCHER_H_
