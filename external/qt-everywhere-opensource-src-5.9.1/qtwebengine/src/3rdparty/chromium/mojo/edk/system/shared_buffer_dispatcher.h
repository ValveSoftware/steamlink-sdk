// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_SHARED_BUFFER_DISPATCHER_H_
#define MOJO_EDK_SYSTEM_SHARED_BUFFER_DISPATCHER_H_

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/macros.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/dispatcher.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {

namespace edk {
class NodeController;
class PlatformSupport;

class MOJO_SYSTEM_IMPL_EXPORT SharedBufferDispatcher final : public Dispatcher {
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
  // TODO(vtl): This should probably be made to return a scoped_refptr and have
  // a MojoResult out parameter instead.
  static MojoResult Create(
      const MojoCreateSharedBufferOptions& validated_options,
      NodeController* node_controller,
      uint64_t num_bytes,
      scoped_refptr<SharedBufferDispatcher>* result);

  // Create a |SharedBufferDispatcher| from |shared_buffer|.
  static MojoResult CreateFromPlatformSharedBuffer(
      const scoped_refptr<PlatformSharedBuffer>& shared_buffer,
      scoped_refptr<SharedBufferDispatcher>* result);

  // The "opposite" of SerializeAndClose(). Called by Dispatcher::Deserialize().
  static scoped_refptr<SharedBufferDispatcher> Deserialize(
      const void* bytes,
      size_t num_bytes,
      const ports::PortName* ports,
      size_t num_ports,
      PlatformHandle* platform_handles,
      size_t num_platform_handles);

  // Passes the underlying platform shared buffer. This dispatcher must be
  // closed after calling this function.
  scoped_refptr<PlatformSharedBuffer> PassPlatformSharedBuffer();

  // Dispatcher:
  Type GetType() const override;
  MojoResult Close() override;
  MojoResult DuplicateBufferHandle(
      const MojoDuplicateBufferHandleOptions* options,
      scoped_refptr<Dispatcher>* new_dispatcher) override;
  MojoResult MapBuffer(
      uint64_t offset,
      uint64_t num_bytes,
      MojoMapBufferFlags flags,
      std::unique_ptr<PlatformSharedBufferMapping>* mapping) override;
  void StartSerialize(uint32_t* num_bytes,
                      uint32_t* num_ports,
                      uint32_t* num_platform_handles) override;
  bool EndSerialize(void* destination,
                    ports::PortName* ports,
                    PlatformHandle* handles) override;
  bool BeginTransit() override;
  void CompleteTransitAndClose() override;
  void CancelTransit() override;

 private:
  static scoped_refptr<SharedBufferDispatcher> CreateInternal(
      scoped_refptr<PlatformSharedBuffer> shared_buffer) {
    return make_scoped_refptr(
        new SharedBufferDispatcher(std::move(shared_buffer)));
  }

  explicit SharedBufferDispatcher(
      scoped_refptr<PlatformSharedBuffer> shared_buffer);
  ~SharedBufferDispatcher() override;

  // Validates and/or sets default options for
  // |MojoDuplicateBufferHandleOptions|. If non-null, |in_options| must point to
  // a struct of at least |in_options->struct_size| bytes. |out_options| must
  // point to a (current) |MojoDuplicateBufferHandleOptions| and will be
  // entirely overwritten on success (it may be partly overwritten on failure).
  static MojoResult ValidateDuplicateOptions(
      const MojoDuplicateBufferHandleOptions* in_options,
      MojoDuplicateBufferHandleOptions* out_options);

  // Guards access to |shared_buffer_|.
  base::Lock lock_;

  bool in_transit_ = false;

  // We keep a copy of the buffer's platform handle during transit so we can
  // close it if something goes wrong.
  ScopedPlatformHandle handle_for_transit_;

  scoped_refptr<PlatformSharedBuffer> shared_buffer_;

  DISALLOW_COPY_AND_ASSIGN(SharedBufferDispatcher);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_SHARED_BUFFER_DISPATCHER_H_
